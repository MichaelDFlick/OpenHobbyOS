#include "blkdev.h"
#include "ata.h"
#include "vfs.h"

#include "abi/linux.h"
#include "console.h"
#include "cpuid.h"
#include "format.h"
#include "initrd.h"
#include "mbr.h"
#include "memory.h"
#include "paging.h"
#include "panic.h"
#include "pit.h"
#include "task.h"
#include "string.h"


struct vfs_node {
    u64 inode;
    char name[VFS_NAME_MAX];
    char path[VFS_PATH_MAX];
    bool is_dir;
    u32 mode;
    u32 uid;
    u32 gid;
    u32 virtual_id;
    enum vfs_type type;
    const initrd_file_t *file;
    vfs_ramfile_t *ramfile;
    u32 ext2_inode;
    struct vfs_node *parent;
    struct vfs_node *sibling;
    struct vfs_node *child;
};

static struct vfs_node *root_node;
static bool mounted;
static u64 next_inode = 1;
static struct ext2_fs root_fs;
static bool has_ext2_root = false;

static void vfs_overlay_initrd(void);
static void vfs_bootstrap_standard_dirs(void);

enum {
    VFS_VIRTUAL_NONE = 0,
    VFS_VIRTUAL_PROC_UPTIME,
    VFS_VIRTUAL_PROC_LOADAVG,
    VFS_VIRTUAL_PROC_MEMINFO,
    VFS_VIRTUAL_PROC_CPUINFO,
    VFS_VIRTUAL_PROC_MOUNTS,
    VFS_VIRTUAL_PROC_ROUTE,
    VFS_VIRTUAL_DEV_NULL,
    VFS_VIRTUAL_DEV_TTY,
    VFS_VIRTUAL_DEV_FB0,
    VFS_VIRTUAL_DEV_NET,
    VFS_VIRTUAL_DEV_KEYBOARD,
    VFS_VIRTUAL_DEV_MOUSE,
};

struct ext2_fs *vfs_ext2_fs(void) {
    return has_ext2_root ? &root_fs : NULL;
}

bool vfs_has_ext2_root(void) {
    return has_ext2_root;
}

enum vfs_type vfs_node_type(const vfs_node_t *node) {
    if (!node) return VFS_TYPE_NONE;
    return node->type;
}

bool vfs_node_is_ext2(const vfs_node_t *node) {
    return node && node->type == VFS_TYPE_EXT2;
}

static bool path_segment(const char **cursor, char *segment, size_t segment_size) {
    size_t used = 0;

    while (**cursor == '/') {
        (*cursor)++;
    }
    if (**cursor == '\0') {
        return false;
    }

    while (**cursor && **cursor != '/') {
        if (used + 1 >= segment_size) {
            panic("VFS segment is too long");
        }
        segment[used++] = **cursor;
        (*cursor)++;
    }
    segment[used] = '\0';
    return true;
}

static void free_ramfile(vfs_ramfile_t *ramfile) {
    if (!ramfile) {
        return;
    }

    if (ramfile->backing_allocation) {
        kfree(ramfile->backing_allocation);
    } else if (ramfile->data) {
        kfree(ramfile->data);
    }
    kfree(ramfile);
}

#define RAMFILE_PAGE_ALIGN 4096u

static char *alloc_ramfile_buffer(u32 capacity, void **raw_out) {
    void *raw;
    uintptr_t aligned;

    raw = kmalloc(capacity + RAMFILE_PAGE_ALIGN);
    if (!raw) {
        return NULL;
    }

    aligned = ((uintptr_t)raw + RAMFILE_PAGE_ALIGN - 1u) & ~(uintptr_t)(RAMFILE_PAGE_ALIGN - 1u);
    memset((void *)aligned, 0, capacity);
    *raw_out = raw;
    return (char *)aligned;
}

static bool data_looks_executable(const void *data, u32 size) {
    const u8 *bytes = (const u8 *)data;
    return data && size >= 4 &&
           bytes[0] == 0x7F &&
           bytes[1] == 'E' &&
           bytes[2] == 'L' &&
           bytes[3] == 'F';
}

static bool looks_executable(const initrd_file_t *file) {
    return file && data_looks_executable(file->data, file->size);
}

static const task_state_t *vfs_current_task_state(void) {
    const task_state_t *state = task_state();
    return state ? state : NULL;
}

static void vfs_set_node_identity(struct vfs_node *node, u32 mode, u32 uid, u32 gid) {
    if (!node) {
        return;
    }
    node->mode = mode;
    node->uid = uid;
    node->gid = gid;
}

static void vfs_init_default_identity(struct vfs_node *node, bool is_dir) {
    const task_state_t *state = vfs_current_task_state();
    u32 uid = 0;
    u32 gid = 0;

    if (state) {
        uid = state->uid;
        gid = state->gid;
    }

    vfs_set_node_identity(node, is_dir ? 0755u : 0644u, uid, gid);
}

static void vfs_init_system_identity(struct vfs_node *node, bool is_dir) {
    vfs_set_node_identity(node, is_dir ? 0755u : 0644u, 0, 0);
}

static bool vfs_path_is_tmp(const char *path) {
    return path && (strncmp(path, "/tmp/", 5) == 0 || strcmp(path, "/tmp") == 0);
}

static size_t append_text(char *buffer, size_t size, size_t used, const char *text) {
    while (used + 1 < size && *text) {
        buffer[used++] = *text++;
    }
    if (size != 0) {
        buffer[used < size ? used : size - 1] = '\0';
    }
    return used;
}

static size_t append_u32(char *buffer, size_t size, size_t used, u32 value) {
    char digits[16];
    size_t count = 0;

    if (value == 0) {
        return append_text(buffer, size, used, "0");
    }

    while (value && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (used + 1 < size && count > 0) {
        buffer[used++] = digits[--count];
    }
    if (size != 0) {
        buffer[used < size ? used : size - 1] = '\0';
    }
    return used;
}

static void build_path(struct vfs_node *node, char *buffer, size_t size) {
    if (!node || size == 0) {
        if (size > 0) buffer[0] = '\0';
        return;
    }

    if (node->parent == NULL) {
        buffer[0] = '/';
        buffer[1] = '\0';
        return;
    }

    build_path(node->parent, buffer, size);
    size_t len = strlen(buffer);

    if (len + 1 < size) {
        if (len > 1 || buffer[0] != '/') {
            buffer[len++] = '/';
        }
        size_t i = 0;
        while (len < size - 1 && node->name[i]) {
            buffer[len++] = node->name[i++];
        }
        buffer[len] = '\0';
    }
}

static struct vfs_node *new_node(const char *name, bool is_dir) {
    struct vfs_node *node = kcalloc(1, sizeof(struct vfs_node));
    if (!node) {
        panic("Out of memory while creating VFS node");
    }
    node->inode = next_inode++;
    strncpy(node->name, name, VFS_NAME_MAX - 1);
    node->name[VFS_NAME_MAX - 1] = '\0';
    node->is_dir = is_dir;
    node->type = VFS_TYPE_RAMFS;
    vfs_init_default_identity(node, is_dir);
    return node;
}

static void add_child(struct vfs_node *parent, struct vfs_node *child) {
    child->parent = parent;
    child->sibling = parent->child;
    parent->child = child;
}

static struct vfs_node *find_child(struct vfs_node *parent, const char *name) {
    struct vfs_node *child = parent->child;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->sibling;
    }
    return NULL;
}

static struct vfs_node *find_or_create_directory(struct vfs_node *parent, const char *name) {
    struct vfs_node *child = find_child(parent, name);
    if (child) {
        return child;
    }
    child = new_node(name, true);
    add_child(parent, child);
    return child;
}

static bool create_virtual_nodes(void) {
    static const struct {
        const char *path;
        u32 id;
    } virtual_files[] = {
        {"/proc/uptime", VFS_VIRTUAL_PROC_UPTIME},
        {"/proc/loadavg", VFS_VIRTUAL_PROC_LOADAVG},
        {"/proc/meminfo", VFS_VIRTUAL_PROC_MEMINFO},
        {"/proc/cpuinfo", VFS_VIRTUAL_PROC_CPUINFO},
        {"/proc/mounts", VFS_VIRTUAL_PROC_MOUNTS},
        {"/proc/net/route", VFS_VIRTUAL_PROC_ROUTE},
        {"/dev/null", VFS_VIRTUAL_DEV_NULL},
        {"/dev/tty", VFS_VIRTUAL_DEV_TTY},
        {"/dev/fb0", VFS_VIRTUAL_DEV_FB0},
        {"/dev/net", VFS_VIRTUAL_DEV_NET},
        {"/dev/keyboard", VFS_VIRTUAL_DEV_KEYBOARD},
        {"/dev/mouse", VFS_VIRTUAL_DEV_MOUSE},
    };

    for (size_t i = 0; i < sizeof(virtual_files) / sizeof(virtual_files[0]); ++i) {
        const char *path = virtual_files[i].path;
        struct vfs_node *parent = root_node;
        const char *cursor = path;
        char segment[VFS_NAME_MAX];

        while (path_segment(&cursor, segment, sizeof(segment))) {
            if (*cursor == '\0') {
                struct vfs_node *node = new_node(segment, false);
                node->virtual_id = virtual_files[i].id;
                vfs_init_system_identity(node, false);
                vfs_set_node_identity(node, 0666u, 0, 0);
                add_child(parent, node);
                break;
            } else {
                struct vfs_node *child = find_child(parent, segment);
                if (!child) {
                    child = new_node(segment, true);
                    add_child(parent, child);
                }
                parent = child;
            }
        }
    }
    return true;
}

bool vfs_init_ramfs(void) {
    root_node = new_node("", true);
    root_node->inode = 1;

    if (!initrd_ready()) {
        return true;
    }

    u32 count = initrd_count();
    for (u32 i = 0; i < count; ++i) {
        const initrd_file_t *file = initrd_file_at(i);
        if (!file) continue;

        struct vfs_node *parent = root_node;
        const char *cursor = file->name;
        char segment[VFS_NAME_MAX];

        while (path_segment(&cursor, segment, sizeof(segment))) {
            if (*cursor == '\0') {
                struct vfs_node *node = new_node(segment, false);
                node->file = file;
                vfs_set_node_identity(node, looks_executable(file) ? 0755u : 0644u, 0, 0);
                add_child(parent, node);
                build_path(node, node->path, VFS_PATH_MAX);
                break;
            } else {
                parent = find_or_create_directory(parent, segment);
            }
        }
    }

    create_virtual_nodes();
    mounted = true;
    return true;
}

static bool ext2_build_tree_recursive(struct ext2_fs *fs, struct vfs_node *parent, u32 parent_inode, const char *parent_path) {
    ext2_file_t dir;
    struct ext2_dir_entry entry;
    char name_buf[EXT2_NAME_LEN + 1];

    if (!ext2_opendir(fs, parent_inode, &dir)) {
        return false;
    }

    while (ext2_readdir(&dir, &entry, name_buf)) {
        if (strcmp(name_buf, ".") == 0 || strcmp(name_buf, "..") == 0) {
            continue;
        }

        struct ext2_inode child_inode;
        if (!ext2_read_inode(fs, entry.inode, &child_inode)) {
            continue;
        }

        bool is_dir = ext2_is_dir(&child_inode);
        struct vfs_node *node = new_node(name_buf, is_dir);
        node->type = VFS_TYPE_EXT2;
        node->ext2_inode = entry.inode;
        node->mode = child_inode.i_mode & 07777u;
        node->uid = child_inode.i_uid;
        node->gid = child_inode.i_gid;
        add_child(parent, node);

        char new_path[256];
        if (parent_path[0] == '/' && parent_path[1] == '\0') {
            snprintf(new_path, sizeof(new_path), "/%s", name_buf);
        } else {
            snprintf(new_path, sizeof(new_path), "%s/%s", parent_path, name_buf);
        }
        strncpy(node->path, new_path, VFS_PATH_MAX - 1);
        node->path[VFS_PATH_MAX - 1] = '\0';

        if (is_dir) {
            ext2_build_tree_recursive(fs, node, entry.inode, new_path);
        }
    }

    ext2_closedir(&dir);
    return true;
}

static bool vfs_init_ext2(u32 blkdev_id, u32 partition_start) {
    if (!ext2_mount(blkdev_id, partition_start, &root_fs)) {
        return false;
    }

    root_node = new_node("", true);
    root_node->inode = 1;
    root_node->type = VFS_TYPE_EXT2;
    root_node->ext2_inode = 2;

    ext2_build_tree_recursive(&root_fs, root_node, 2, "/");

    create_virtual_nodes();

    has_ext2_root = true;
    mounted = true;

    console_printf("[vfs] mounted ext2 as root filesystem\n");
    return true;
}

bool vfs_init_from_initrd(void) {
    blkdev_init();
    ata_init();
    if (!vfs_init_ramfs()) {
        return false;
    }
    vfs_bootstrap_standard_dirs();
    return true;
}

bool vfs_init_from_ext2(u32 blkdev_id, u32 partition_start) {
    blkdev_init();
    ata_init();
    if (!vfs_init_ext2(blkdev_id, partition_start)) {
        return false;
    }
    vfs_overlay_initrd();
    vfs_bootstrap_standard_dirs();
    return true;
}

/* Initrd holds the canonical userland image; merge files not already on ext2. */
static void vfs_overlay_initrd(void) {
    u32 i;

    if (!initrd_ready()) {
        return;
    }

    for (i = 0; i < initrd_count(); i++) {
        const initrd_file_t *file = initrd_file_at(i);
        struct vfs_node *parent;
        const char *cursor;
        char segment[VFS_NAME_MAX];

        if (!file) {
            continue;
        }

        parent = root_node;
        cursor = file->name;
        while (path_segment(&cursor, segment, sizeof(segment))) {
            if (*cursor == '\0') {
                if (find_child(parent, segment)) {
                    break;
                }
                {
                    struct vfs_node *node = new_node(segment, false);
                    node->type = VFS_TYPE_RAMFS;
                    node->file = file;
                    vfs_set_node_identity(node, looks_executable(file) ? 0755u : 0644u, 0, 0);
                    add_child(parent, node);
                    build_path(node, node->path, VFS_PATH_MAX);
                }
                break;
            }
            {
                struct vfs_node *next = find_child(parent, segment);
                if (next) {
                    if (!next->is_dir) {
                        break;
                    }
                    parent = next;
                } else {
                    parent = find_or_create_directory(parent, segment);
                }
            }
        }
    }
}

static void vfs_ensure_directory_tree(const char *path, u32 mode, u32 uid, u32 gid) {
    struct vfs_node *parent = root_node;
    const char *cursor = path;
    char segment[VFS_NAME_MAX];

    if (!path || !root_node) {
        return;
    }

    while (path_segment(&cursor, segment, sizeof(segment))) {
        struct vfs_node *child = find_child(parent, segment);

        if (!child) {
            child = new_node(segment, true);
            vfs_set_node_identity(child, mode & 07777u, uid, gid);
            add_child(parent, child);
        }

        if (*cursor == '\0') {
            vfs_set_node_identity(child, mode & 07777u, uid, gid);
            return;
        }

        if (!child->is_dir) {
            return;
        }

        parent = child;
    }
}

static void vfs_bootstrap_standard_dirs(void) {
    vfs_ensure_directory_tree("/tmp", 0777u, 0, 0);
    vfs_ensure_directory_tree("/home", 0755u, 0, 0);
    vfs_ensure_directory_tree("/home/user", 0700u, 1000u, 1000u);
    vfs_ensure_directory_tree("/root", 0700u, 0, 0);
}

void vfs_init(void) {
    blkdev_init();
    ata_init();

    mbr_partition_info_t partitions[MBR_PARTITION_COUNT];
    if (mbr_read(0, partitions)) {
        for (int i = 0; i < MBR_PARTITION_COUNT; i++) {
            if (partitions[i].present && partitions[i].type == MBR_TYPE_LINUX_NATIVE) {
                if (ext2_is_valid(0, partitions[i].lba_start)) {
                    if (vfs_init_ext2(0, partitions[i].lba_start)) {
                        vfs_overlay_initrd();
                        vfs_bootstrap_standard_dirs();
                        console_printf("[vfs] mounted ext2 root\n");
                        return;
                    }
                }
            }
        }
    }

    console_printf("[vfs] no ext2 root found, using initrd\n");
    vfs_init_ramfs();
    vfs_bootstrap_standard_dirs();
}

bool vfs_ready(void) {
    return mounted;
}

const vfs_node_t *vfs_root(void) {
    return root_node;
}

/* 
 * vfs_resolve: The recursive descent into path-parsing madness.
 * We take a string and attempt to find a node in our tree. 
 * This is where we handle mount points, '..' traversals, 
 * and the general messy reality of userspace paths.
 */
const vfs_node_t *vfs_resolve(const vfs_node_t *cwd, const char *path) {
    if (!path || !mounted) {
        return NULL;
    }

    /* Start at '/' for absolute paths, or 'cwd' for relative ones. */
    struct vfs_node *base = (path[0] == '/') ? root_node : (struct vfs_node *)cwd;
    if (!base) {
        base = root_node;
    }

    /* Quick exit for the root directory. */
    if (path[0] == '/' && path[1] == '\0') {
        return root_node;
    }

    const char *cursor = path;
    char segment[VFS_NAME_MAX];

    /* 
     * Walk the path segment by segment. 
     * We don't support symbolic links yet, so this is a straightforward 
     * linear traversal. If we hit a segment that doesn't exist, we bail.
     */
    while (path_segment(&cursor, segment, sizeof(segment))) {
        if (strcmp(segment, ".") == 0) {
            /* Dot means 'here'. Do nothing. */
            continue;
        } else if (strcmp(segment, "..") == 0) {
            /* Dot-dot means 'up'. If we're at root, we just stay at root. */
            if (base->parent) {
                base = base->parent;
            }
        } else {
            /* Search children. Linear search is fine for a hobby OS. */
            struct vfs_node *child = find_child(base, segment);
            if (!child) {
                return NULL; /* Path component not found. */
            }
            base = child;
        }
    }

    return base;
}

int vfs_mkdir(const char *path, u32 mode, u32 uid, u32 gid) {
    if (!path || !mounted || (has_ext2_root && !vfs_path_is_tmp(path))) {
        return -1;
    }

    struct vfs_node *parent = root_node;
    const char *cursor = path;
    char segment[VFS_NAME_MAX];

    while (path_segment(&cursor, segment, sizeof(segment))) {
        if (*cursor == '\0') {
            if (find_child(parent, segment)) {
                return -1;
            }
            struct vfs_node *node = new_node(segment, true);
            vfs_set_node_identity(node, mode & 07777u, uid, gid);
            add_child(parent, node);
            return 0;
        } else {
            struct vfs_node *child = find_child(parent, segment);
            if (!child) {
                child = new_node(segment, true);
                add_child(parent, child);
            }
            parent = child;
        }
    }

    return -1;
}

const vfs_node_t *vfs_parent(const vfs_node_t *node) {
    return node ? node->parent : NULL;
}

const char *vfs_name(const vfs_node_t *node) {
    return node ? node->name : NULL;
}

const char *vfs_path(const vfs_node_t *node) {
    if (!node) return NULL;
    struct vfs_node *n = (struct vfs_node *)node;
    if (n->path[0] == '\0') {
        build_path(n, n->path, VFS_PATH_MAX);
    }
    return n->path;
}

u64 vfs_inode(const vfs_node_t *node) {
    return node ? node->inode : 0;
}

bool vfs_is_dir(const vfs_node_t *node) {
    return node ? node->is_dir : false;
}

bool vfs_is_file(const vfs_node_t *node) {
    return node ? !node->is_dir : false;
}

const initrd_file_t *vfs_backing_file(const vfs_node_t *node) {
    if (!node || node->type != VFS_TYPE_RAMFS) {
        return NULL;
    }
    return node->file;
}

u32 vfs_file_size(const vfs_node_t *node) {
    if (!node || node->is_dir || node->virtual_id != VFS_VIRTUAL_NONE) {
        return 0;
    }

    if (node->type == VFS_TYPE_EXT2) {
        struct ext2_inode inode;
        if (ext2_read_inode(&root_fs, node->ext2_inode, &inode)) {
            return ext2_file_size(&inode);
        }
        return 0;
    }

    if (node->ramfile) {
        return node->ramfile->size;
    }

    return node->file ? node->file->size : 0;
}

ssize_t vfs_read(const vfs_node_t *node, u32 offset, void *buffer, size_t length) {
    if (!node || node->is_dir || !buffer || length == 0) {
        return -1;
    }

    if (node->virtual_id != VFS_VIRTUAL_NONE) {
        char buf[1024];
        size_t count = 0;

        switch (node->virtual_id) {
            case VFS_VIRTUAL_PROC_UPTIME: {
                extern u32 pit_ticks(void);
                extern u32 pit_frequency(void);
                u32 hz = pit_frequency();
                if (hz == 0) hz = 100;
                u32 ticks = pit_ticks();
                u32 secs = ticks / hz;
                u32 centisecs = (ticks % hz) * 100 / hz;
                count = append_u32(buf, sizeof(buf), 0, secs);
                count = append_text(buf, sizeof(buf), count, ".");
                if (centisecs < 10) count = append_text(buf, sizeof(buf), count, "0");
                count = append_u32(buf, sizeof(buf), count, centisecs);
                count = append_text(buf, sizeof(buf), count, " ");
                count = append_u32(buf, sizeof(buf), count, secs);
                count = append_text(buf, sizeof(buf), count, ".");
                if (centisecs < 10) count = append_text(buf, sizeof(buf), count, "0");
                count = append_u32(buf, sizeof(buf), count, centisecs);
                count = append_text(buf, sizeof(buf), count, "\n");
                break;
            }
            case VFS_VIRTUAL_PROC_LOADAVG: {
                int total = task_count_total();
                int runnable = task_count_runnable();
                int last_pid = 1;
                {
                    extern int task_active_slot_index(void);
                    int idx = task_active_slot_index();
                    if (idx >= 0) {
                        extern int task_slot_pid(int);
                        last_pid = task_slot_pid(idx);
                        if (last_pid < 1) last_pid = 1;
                    }
                }
                count = append_text(buf, sizeof(buf), 0, "0.00 0.00 0.00 ");
                count = append_u32(buf, sizeof(buf), count, runnable);
                count = append_text(buf, sizeof(buf), count, "/");
                count = append_u32(buf, sizeof(buf), count, total);
                count = append_text(buf, sizeof(buf), count, " ");
                count = append_u32(buf, sizeof(buf), count, last_pid);
                count = append_text(buf, sizeof(buf), count, "\n");
                break;
            }
            case VFS_VIRTUAL_PROC_MEMINFO: {
                extern u32 frame_total(void);
                extern u32 frame_free_count(void);
                u32 total_frames = frame_total();
                u32 free_frames = frame_free_count();
                u32 mem_total_kb = total_frames * 4;
                u32 mem_free_kb = free_frames * 4;
                count = append_text(buf, sizeof(buf), 0, "MemTotal:       ");
                count = append_u32(buf, sizeof(buf), count, mem_total_kb);
                count = append_text(buf, sizeof(buf), count, " kB\nMemFree:        ");
                count = append_u32(buf, sizeof(buf), count, mem_free_kb);
                count = append_text(buf, sizeof(buf), count, " kB\nMemAvailable:   ");
                count = append_u32(buf, sizeof(buf), count, mem_free_kb > 1024 ? mem_free_kb : 0);
                count = append_text(buf, sizeof(buf), count, " kB\nBuffers:        0 kB\nCached:         0 kB\nSwapTotal:      0 kB\nSwapFree:       0 kB\n");
                break;
            }
            case VFS_VIRTUAL_PROC_CPUINFO: {
                const cpu_info_t *cpu = cpu_get_info();
                if (!cpu) {
                    return 0;
                }
                count = append_text(buf, sizeof(buf), 0, "processor\t: 0\n");
                count = append_text(buf, sizeof(buf), count, "vendor_id\t: ");
                count = append_text(buf, sizeof(buf), count, cpu->vendor);
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "cpu family\t: ");
                count = append_u32(buf, sizeof(buf), count, cpu->family);
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "model\t\t: ");
                count = append_u32(buf, sizeof(buf), count, cpu->model);
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "model name\t: ");
                if (cpu->brand_string_valid) {
                    count = append_text(buf, sizeof(buf), count, cpu->brand);
                } else {
                    count = append_text(buf, sizeof(buf), count, cpu->vendor);
                    count = append_text(buf, sizeof(buf), count, " CPU");
                }
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "stepping\t: ");
                count = append_u32(buf, sizeof(buf), count, cpu->stepping);
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "cpu MHz\t\t: 0.000\n");
                if (cpu->cache_size_kb) {
                    count = append_text(buf, sizeof(buf), count, "cache size\t: ");
                    count = append_u32(buf, sizeof(buf), count, cpu->cache_size_kb);
                    count = append_text(buf, sizeof(buf), count, " KB\n");
                }
                count = append_text(buf, sizeof(buf), count,
                    "physical id\t: 0\n"
                    "siblings\t: 1\n"
                    "core id\t\t: 0\n"
                    "cpu cores\t: 1\n"
                    "apicid\t\t: 0\n"
                    "initial apicid\t: 0\n"
                    "fpu\t\t: ");
                count = append_text(buf, sizeof(buf), count,
                    (cpu->features_edx & CPUID_FEATURE_EDX_FPU) ? "yes" : "no");
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "fpu_exception\t: ");
                count = append_text(buf, sizeof(buf), count,
                    (cpu->features_edx & CPUID_FEATURE_EDX_FPU) ? "yes" : "no");
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "cpuid level\t: ");
                count = append_u32(buf, sizeof(buf), count, cpu->max_leaf);
                count = append_text(buf, sizeof(buf), count, "\n");
                count = append_text(buf, sizeof(buf), count, "wp\t\t: yes\n");
                {
                    static const struct {
                        u32 reg_idx;  /* 0 = EDX, 1 = ECX */
                        u32 bit;
                        const char *name;
                    } flag_map[] = {
                        {0, 0,  "fpu"},    {0, 1,  "vme"},
                        {0, 2,  "de"},     {0, 3,  "pse"},
                        {0, 4,  "tsc"},    {0, 5,  "msr"},
                        {0, 6,  "pae"},    {0, 7,  "mce"},
                        {0, 8,  "cx8"},    {0, 9,  "apic"},
                        {0, 11, "sep"},    {0, 12, "mtrr"},
                        {0, 13, "pge"},    {0, 14, "mca"},
                        {0, 15, "cmov"},   {0, 16, "pat"},
                        {0, 17, "pse36"},  {0, 19, "clflush"},
                        {0, 21, "dtes"},   {0, 22, "acpi"},
                        {0, 23, "mmx"},    {0, 24, "fxsr"},
                        {0, 25, "sse"},    {0, 26, "sse2"},
                        {0, 27, "ss"},     {0, 28, "ht"},
                        {0, 29, "tm"},     {0, 31, "pbe"},
                        {1, 0,  "pni"},    {1, 1,  "pclmulqdq"},
                        {1, 2,  "dtes64"}, {1, 3,  "monitor"},
                        {1, 4,  "ds_cpl"}, {1, 5,  "vmx"},
                        {1, 6,  "smx"},    {1, 7,  "est"},
                        {1, 8,  "tm2"},    {1, 9,  "ssse3"},
                        {1, 12, "fma"},    {1, 13, "cx16"},
                        {1, 14, "xtpr"},   {1, 15, "pdcm"},
                        {1, 17, "pcid"},   {1, 18, "dca"},
                        {1, 19, "sse4_1"},{1, 20, "sse4_2"},
                        {1, 21, "x2apic"},{1, 22, "movbe"},
                        {1, 23, "popcnt"},{1, 24, "tsc_deadline_timer"},
                        {1, 25, "aes"},   {1, 26, "xsave"},
                        {1, 28, "avx"},   {1, 29, "f16c"},
                        {1, 30, "rdrand"},{1, 31, "hypervisor"},
                    };
                    count = append_text(buf, sizeof(buf), count, "flags\t\t:");
                    for (size_t i = 0; i < sizeof(flag_map) / sizeof(flag_map[0]); i++) {
                        u32 reg_val = flag_map[i].reg_idx ? cpu->features_ecx : cpu->features_edx;
                        if (reg_val & (1u << flag_map[i].bit)) {
                            count = append_text(buf, sizeof(buf), count, " ");
                            count = append_text(buf, sizeof(buf), count, flag_map[i].name);
                        }
                    }
                    count = append_text(buf, sizeof(buf), count, "\n");
                }
                count = append_text(buf, sizeof(buf), count, "bogomips\t: 0.00\n");
                break;
            }
            default:
                return 0;
        }

        if (offset >= count) {
            return 0;
        }
        if (offset + length > count) {
            length = count - offset;
        }
        memcpy(buffer, buf + offset, length);
        return (ssize_t)length;
    }

    if (node->type == VFS_TYPE_EXT2) {
        ext2_file_t file;
        ssize_t total;

        if (!ext2_open(&root_fs, node->ext2_inode, &file)) {
            return -1;
        }

        file.offset = offset;
        file.current_block = 0xFFFFFFFF;

        total = ext2_read(&file, buffer, length);
        ext2_close(&file);
        return total;
    }

    if (node->ramfile) {
        if (offset >= node->ramfile->size) {
            return 0;
        }
        u32 available = node->ramfile->size - offset;
        if (length > available) {
            length = available;
        }
        memcpy(buffer, node->ramfile->data + offset, length);
        return (ssize_t)length;
    }

    if (!node->file) {
        return -1;
    }

    if (offset >= node->file->size) {
        return 0;
    }

    u32 available = node->file->size - offset;
    if (length > available) {
        length = available;
    }

    memcpy(buffer, node->file->data + offset, length);
    return (ssize_t)length;
}

bool vfs_stat_node(const vfs_node_t *node, vfs_stat_t *stat) {
    if (!node || !stat) {
        return false;
    }

    memset(stat, 0, sizeof(*stat));
    stat->inode = node->inode;
    stat->is_dir = node->is_dir;
    stat->block_size = 512;
    stat->size = node->is_dir ? 0 : vfs_file_size(node);
    stat->blocks = (stat->size + 511u) / 512u;
    stat->nlink = node->is_dir ? 2u : (node->ramfile ? node->ramfile->links : 1u);
    stat->uid = node->uid;
    stat->gid = node->gid;

    if (node->is_dir) {
        stat->mode = LINUX_S_IFDIR | (node->mode & 07777u);
    } else if (node->virtual_id == VFS_VIRTUAL_DEV_NULL || node->virtual_id == VFS_VIRTUAL_DEV_TTY || node->virtual_id == VFS_VIRTUAL_DEV_FB0 || node->virtual_id == VFS_VIRTUAL_DEV_KEYBOARD || node->virtual_id == VFS_VIRTUAL_DEV_MOUSE) {
        stat->mode = LINUX_S_IFCHR | (node->mode & 07777u);
        stat->block_size = 1;
    } else if (node->ramfile) {
        stat->mode = LINUX_S_IFREG | (node->mode & 07777u);
    } else if (node->type == VFS_TYPE_EXT2) {
        stat->mode = (node->is_dir ? LINUX_S_IFDIR : LINUX_S_IFREG) | (node->mode & 07777u);
    } else if (node->file) {
        stat->mode = LINUX_S_IFREG | (node->mode & 07777u);
    } else {
        stat->mode = LINUX_S_IFREG | (node->mode & 07777u);
    }
    return true;
}

u32 vfs_child_count(const vfs_node_t *node) {
    const struct vfs_node *child = node ? node->child : NULL;
    u32 count = 0;

    while (child) {
        count++;
        child = child->sibling;
    }

    return count;
}

const vfs_node_t *vfs_child_at(const vfs_node_t *node, u32 index) {
    const struct vfs_node *child = node ? node->child : NULL;

    while (child) {
        if (index == 0) {
            return child;
        }
        index--;
        child = child->sibling;
    }

    return NULL;
}

bool vfs_node_is_ramfile(const vfs_node_t *node) {
    return node && node->ramfile != NULL;
}

vfs_ramfile_t *vfs_node_ramfile(const vfs_node_t *node) {
    return node ? node->ramfile : NULL;
}

const vfs_node_t *vfs_create_ramfile(const char *path, u32 mode, u32 uid, u32 gid) {
    if (!path || !mounted || (has_ext2_root && !vfs_path_is_tmp(path))) {
        return NULL;
    }

    struct vfs_node *parent = root_node;
    const char *cursor = path;
    char segment[VFS_NAME_MAX];

    while (path_segment(&cursor, segment, sizeof(segment))) {
        if (*cursor == '\0') {
            struct vfs_node *existing = find_child(parent, segment);
            if (existing) {
                return NULL;
            }

            struct vfs_node *node = new_node(segment, false);
            node->ramfile = kcalloc(1, sizeof(vfs_ramfile_t));
            if (!node->ramfile) {
                return NULL;
            }
            node->ramfile->links = 1;
            node->ramfile->capacity = RAMFILE_PAGE_ALIGN;
            node->ramfile->data = alloc_ramfile_buffer(node->ramfile->capacity,
                                                       &node->ramfile->backing_allocation);
            if (!node->ramfile->data) {
                free_ramfile(node->ramfile);
                return NULL;
            }
            vfs_set_node_identity(node, mode & 07777u, uid, gid);
            add_child(parent, node);
            build_path(node, node->path, VFS_PATH_MAX);
            return node;
        } else {
            parent = find_or_create_directory(parent, segment);
        }
    }

    return NULL;
}

ssize_t vfs_write_ramfile(const vfs_node_t *node, u32 offset, const void *buffer, size_t length) {
    if (!node || !buffer || length == 0) {
        return -1;
    }

    if (node->type == VFS_TYPE_EXT2) {
        return -1;
    }

    if (!node->ramfile) {
        return -1;
    }

    u32 required = offset + (u32)length;
    if (required > node->ramfile->capacity) {
        u32 new_capacity = node->ramfile->capacity;
        void *new_raw = NULL;
        char *new_data;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        new_capacity = (new_capacity + RAMFILE_PAGE_ALIGN - 1u) & ~(RAMFILE_PAGE_ALIGN - 1u);
        new_data = alloc_ramfile_buffer(new_capacity, &new_raw);
        if (!new_data) {
            return -1;
        }
        memcpy(new_data, node->ramfile->data, node->ramfile->size);
        if (node->ramfile->backing_allocation) {
            kfree(node->ramfile->backing_allocation);
        } else {
            kfree(node->ramfile->data);
        }
        node->ramfile->data = new_data;
        node->ramfile->backing_allocation = new_raw;
        node->ramfile->capacity = new_capacity;
    }

    if (offset > node->ramfile->size) {
        memset(node->ramfile->data + node->ramfile->size, 0, offset - node->ramfile->size);
    }
    memcpy(node->ramfile->data + offset, buffer, length);

    if (required > node->ramfile->size) {
        node->ramfile->size = required;
    }

    return (ssize_t)length;
}

int vfs_unlink_ramfile(const char *path) {
    if (!path || !mounted || (has_ext2_root && !vfs_path_is_tmp(path))) {
        return -1;
    }

    const vfs_node_t *node = vfs_resolve(NULL, path);
    if (!node || !node->ramfile) {
        return -1;
    }

    if (node->ramfile->links > 1) {
        ((vfs_node_t *)node)->ramfile->links--;
        return 0;
    }

    return -1;
}

int vfs_link_ramfile(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath || !mounted ||
        ((has_ext2_root) && (!vfs_path_is_tmp(oldpath) || !vfs_path_is_tmp(newpath)))) {
        return -1;
    }

    const vfs_node_t *oldnode = vfs_resolve(NULL, oldpath);
    if (!oldnode || !oldnode->ramfile) {
        return -1;
    }

    const vfs_node_t *newnode = vfs_resolve(NULL, newpath);
    if (newnode) {
        return -1;
    }

    struct vfs_node *parent = root_node;
    const char *cursor = newpath;
    char segment[VFS_NAME_MAX];

    while (path_segment(&cursor, segment, sizeof(segment))) {
        if (*cursor == '\0') {
            struct vfs_node *node = new_node(segment, false);
            node->ramfile = ((vfs_node_t *)oldnode)->ramfile;
            node->ramfile->links++;
            add_child(parent, node);
            build_path(node, node->path, VFS_PATH_MAX);
            return 0;
        } else {
            parent = find_or_create_directory(parent, segment);
        }
    }

    return -1;
}

int vfs_rename_ramfile(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath || !mounted ||
        ((has_ext2_root) && (!vfs_path_is_tmp(oldpath) || !vfs_path_is_tmp(newpath)))) {
        return -1;
    }

    if (vfs_link_ramfile(oldpath, newpath) != 0) {
        return -1;
    }

    return vfs_unlink_ramfile(oldpath);
}

int vfs_chmod_path(const char *path, u32 mode) {
    const vfs_node_t *node;

    if (!path || !mounted) {
        return -1;
    }

    node = vfs_resolve(NULL, path);
    if (!node) {
        return -1;
    }

    ((struct vfs_node *)node)->mode = mode & 07777u;
    return 0;
}
