typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef int                i32;
typedef unsigned long long u64;
typedef long long          i64;

#define NULL ((void *)0)

/* ELF32 types */
#define ELF_MAGIC      0x464C457F
#define ET_EXEC        2
#define ET_DYN         3
#define PT_LOAD        1
#define PT_DYNAMIC     2
#define PT_INTERP      3
#define PT_PHDR        6
#define PT_TLS         7
#define PT_GNU_RELRO   0x6474e552
#define DT_NEEDED      1
#define DT_PLTGOT      3
#define DT_HASH        4
#define DT_STRTAB      5
#define DT_SYMTAB      6
#define DT_RELA        7
#define DT_RELASZ      8
#define DT_RELAENT     9
#define DT_STRSZ       10
#define DT_SYMENT      11
#define DT_INIT        12
#define DT_FINI        13
#define DT_INIT_ARRAY  25
#define DT_FINI_ARRAY  26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_FLAGS       30
#define DT_PLTREL      20
#define DT_JMPREL      23
#define DT_PLTRELSZ    2
#define DT_DEBUG       21
#define DT_TEXTREL     22
#define DT_FLAGS_1     0x6ffffffb
#define DT_RELR        0x6ffffffd
#define DT_RELRSZ      0x6ffffffe
#define DT_RELRENT     0x6ffffffc
#define DF_1_NOW       0x00000001

#define R_386_NONE         0
#define R_386_32           1
#define R_386_PC32         2
#define R_386_GLOB_DAT     6
#define R_386_JUMP_SLOT    7
#define R_386_RELATIVE     8

#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
#define MAP_PRIVATE 0x02
#define MAP_FIXED   0x10
#define MAP_ANONYMOUS 0x20

#define AT_NULL     0
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ   6
#define AT_BASE     7
#define AT_ENTRY    9
#define AT_EXECFN   31

#define O_RDONLY    0x0000

#define SYS_EXIT    1
#define SYS_READ    3
#define SYS_WRITE   4
#define SYS_OPEN    5
#define SYS_CLOSE   6
#define SYS_LSEEK   19
#define SYS_MMAP2   192
#define SYS_MUNMAP  91
#define SYS_FSTAT64 197

typedef struct {
    u32 st_name;
    u32 st_value;
    u32 st_size;
    u8  st_info;
    u8  st_other;
    u16 st_shndx;
} elf32_sym_t;

typedef struct {
    u32 r_offset;
    u32 r_info;
} elf32_rel_t;

typedef struct {
    u32 r_offset;
    u32 r_info;
    i32 r_addend;
} elf32_rela_t;

typedef struct {
    u32 d_tag;
    u32 d_val;
} elf32_dyn_t;

typedef struct {
    u32 type;
    u32 offset;
    u32 vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf32_phdr_t;

typedef struct {
    u32 ident[4];
    u32 type;
    u32 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf32_ehdr_t;

typedef struct {
    u32 st_dev;
    u8  __pad0[4];
    u32 __st_ino;
    u32 st_mode;
    u32 st_nlink;
    u32 st_uid;
    u32 st_gid;
    u64 st_rdev;
    u8  __pad3[4];
    i64 st_size;
    u32 st_blksize;
    u64 st_blocks;
    u32 st_atime;
    u32 st_atime_nsec;
    u32 st_mtime;
    u32 st_mtime_nsec;
    u32 st_ctime;
    u32 st_ctime_nsec;
    u64 st_ino;
} linux_stat64_t;

typedef struct lib {
    struct lib *next;
    u32 base;
    u32 load_end;
    u32 dyn_vaddr;
    u32 symtab;
    u32 symtabsz;
    u32 strtab;
    u32 strtabsz;
    u32 init;
    u32 fini;
    u32 init_array;
    u32 init_arraysz;
    u32 fini_array;
    u32 fini_arraysz;
    u32 relro_start;
    u32 relro_end;
} lib_t;

static lib_t *loaded_libs = NULL;

static long syscall(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
    long ret;
    register long r_eax asm("eax") = n;
    register long r_ebx asm("ebx") = a1;
    register long r_ecx asm("ecx") = a2;
    register long r_edx asm("edx") = a3;
    register long r_esi asm("esi") = a4;
    register long r_edi asm("edi") = a5;
    register long r_ebp asm("ebp") = a6;
    asm volatile("int $0x80" : "=a"(ret) : "r"(r_eax), "r"(r_ebx), "r"(r_ecx), "r"(r_edx), "r"(r_esi), "r"(r_edi), "r"(r_ebp) : "memory");
    return ret;
}

static int open(const char *path, int flags) { return (int)syscall(SYS_OPEN, (long)path, flags, 0, 0, 0, 0); }
static int close(int fd) { return (int)syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0); }
static int lseek(int fd, int offset, int whence) { return (int)syscall(SYS_LSEEK, fd, offset, whence, 0, 0, 0); }
static int fstat(int fd, linux_stat64_t *st) { return (int)syscall(SYS_FSTAT64, fd, (long)st, 0, 0, 0, 0); }
static int read(int fd, void *buf, unsigned int count) { return (int)syscall(SYS_READ, fd, (long)buf, count, 0, 0, 0); }
static int write(int fd, const void *buf, unsigned int count) { return (int)syscall(SYS_WRITE, fd, (long)buf, count, 0, 0, 0); }
static void exit(int code) { syscall(SYS_EXIT, code, 0, 0, 0, 0, 0); }
static void *mmap(void *addr, unsigned int length, int prot, int flags, int fd, unsigned int page_offset)
{
    return (void *)syscall(SYS_MMAP2, (long)addr, length, prot, flags, fd, page_offset);
}
static int munmap(void *addr, unsigned int length) { return (int)syscall(SYS_MUNMAP, (long)addr, length, 0, 0, 0, 0); }

static int streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static u32 align_up(u32 v, u32 a)
{
    return (v + a - 1) & ~(a - 1);
}

/* Read ELF header and program headers from a file descriptor.
 * Returns 0 on success. */
static int read_elf_headers(int fd, elf32_ehdr_t *ehdr, elf32_phdr_t **phdrs_out, u32 *phdrs_alloc)
{
    if (read(fd, ehdr, sizeof(*ehdr)) != sizeof(*ehdr))
        return -1;
    if (ehdr->ident[0] != 0x7F || ehdr->ident[1] != 'E' || ehdr->ident[2] != 'L' || ehdr->ident[3] != 'F')
        return -1;
    if (ehdr->type != ET_DYN && ehdr->type != ET_EXEC)
        return -1;
    *phdrs_alloc = ehdr->phnum * ehdr->phentsize;
    lseek(fd, ehdr->phoff, 0);
    if (read(fd, *phdrs_out, *phdrs_alloc) != (int)*phdrs_alloc)
        return -1;
    return 0;
}

/* Load a shared library from the given path. Returns a lib_t entry or NULL. */
static lib_t *load_library(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    elf32_ehdr_t ehdr;
    elf32_phdr_t phdrs_buf[64];
    elf32_phdr_t *phdrs = phdrs_buf;
    u32 phdrs_sz;

    if (read_elf_headers(fd, &ehdr, &phdrs, &phdrs_sz) < 0) {
        close(fd);
        return NULL;
    }

    /* For ET_DYN, the vaddr base is 0; we load at whatever mmap picks. */
    u32 bias = 0;
    u32 load_end = 0;
    u32 first_load_vaddr = 0xFFFFFFFF;
    u32 dyn_vaddr = 0;
    u32 relro_start = 0, relro_end = 0;

    /* First pass: figure out the load bias. For ET_DYN, we map the first
     * PT_LOAD to get its vaddr, then calculate bias. */
    if (ehdr.type == ET_DYN) {
        for (u16 i = 0; i < ehdr.phnum; i++) {
            elf32_phdr_t *ph = &phdrs[i];
            if (ph->type == PT_LOAD) {
                first_load_vaddr = ph->vaddr & 0xFFFFF000;
                /* mmap the whole file to get a base */
                u32 map_size = align_up(ph->filesz, 4096);
                void *m = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, ph->offset / 4096);
                if (m == (void *)-1) {
                    close(fd);
                    return NULL;
                }
                bias = (u32)(unsigned long)m - first_load_vaddr;
                break;
            }
        }
        if (bias == 0 && first_load_vaddr != 0) {
            close(fd);
            return NULL;
        }
    }

    /* Second pass: map all PT_LOAD segments at the correct bias */
    for (u16 i = 0; i < ehdr.phnum; i++) {
        elf32_phdr_t *ph = &phdrs[i];
        if (ph->type == PT_LOAD) {
            u32 vaddr = (ph->vaddr + bias) & 0xFFFFF000;
            u32 memsz = align_up(ph->vaddr + ph->memsz + bias, 4096) - vaddr;
            u32 file_offset = ph->offset;
            int prot = PROT_READ;
            if (ph->flags & 1) prot |= PROT_EXEC;
            if (ph->flags & 2) prot |= PROT_WRITE;

            if (ehdr.type == ET_EXEC) {
                void *m = mmap((void *)vaddr, memsz, prot | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, file_offset / 4096);
                if (m == (void *)-1) {
                    close(fd);
                    return NULL;
                }
            } else {
                void *m = mmap((void *)vaddr, memsz, prot | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, file_offset / 4096);
                if (m == (void *)-1) {
                    close(fd);
                    return NULL;
                }
            }
            u32 seg_end = align_up(ph->vaddr + ph->memsz + bias, 4096);
            if (seg_end > load_end) load_end = seg_end;
        }
        if (ph->type == PT_DYNAMIC) {
            dyn_vaddr = ph->vaddr + bias;
        }
        if (ph->type == PT_GNU_RELRO) {
            relro_start = ph->vaddr + bias;
            relro_end = ph->vaddr + ph->memsz + bias;
        }
    }

    close(fd);

    /* Allocate and fill lib_t */
    lib_t *lib = (lib_t *)(unsigned long)mmap(NULL, sizeof(lib_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (lib == (void *)-1) return NULL;
    lib->base = bias;
    lib->load_end = load_end;
    lib->dyn_vaddr = dyn_vaddr;
    lib->relro_start = relro_start;
    lib->relro_end = relro_end;
    lib->init = 0;
    lib->fini = 0;
    lib->init_array = 0;
    lib->init_arraysz = 0;
    lib->fini_array = 0;
    lib->fini_arraysz = 0;
    lib->symtab = 0;
    lib->symtabsz = 0;
    lib->strtab = 0;
    lib->strtabsz = 0;

    /* Scan dynamic section */
    if (dyn_vaddr) {
        elf32_dyn_t *dyn = (elf32_dyn_t *)dyn_vaddr;
        for (; dyn->d_tag != 0; dyn++) {
            switch (dyn->d_tag) {
            case DT_SYMTAB: lib->symtab = dyn->d_val + bias; break;
            case DT_SYMENT: lib->symtabsz = dyn->d_val; break;
            case DT_STRTAB: lib->strtab = dyn->d_val + bias; break;
            case DT_STRSZ:  lib->strtabsz = dyn->d_val; break;
            case DT_INIT:   lib->init = dyn->d_val + bias; break;
            case DT_FINI:   lib->fini = dyn->d_val + bias; break;
            case DT_INIT_ARRAY:   lib->init_array = dyn->d_val + bias; break;
            case DT_INIT_ARRAYSZ: lib->init_arraysz = dyn->d_val; break;
            case DT_FINI_ARRAY:   lib->fini_array = dyn->d_val + bias; break;
            case DT_FINI_ARRAYSZ: lib->fini_arraysz = dyn->d_val; break;
            }
        }
    }

    lib->next = loaded_libs;
    loaded_libs = lib;
    return lib;
}

/* Find a symbol by name across all loaded libraries.
 * Returns the symbol value (absolute address) or 0 if not found. */
static u32 find_sym(const char *name)
{
    for (lib_t *lib = loaded_libs; lib; lib = lib->next) {
        if (!lib->symtab || !lib->strtab) continue;
        u32 nsyms = lib->symtabsz >= 16 ? lib->symtabsz / 16 : 0;
        elf32_sym_t *syms = (elf32_sym_t *)lib->symtab;
        const char *strs = (const char *)lib->strtab;
        for (u32 i = 0; i < nsyms; i++) {
            if (syms[i].st_value && syms[i].st_name) {
                if (streq(strs + syms[i].st_name, name)) {
                    return syms[i].st_value + lib->base;
                }
            }
        }
    }
    return 0;
}

/* Process RELA relocations for a given library (bias applied). */
static void process_rela(u32 rela_vaddr, u32 rela_sz, u32 bias, u32 plt)
{
    elf32_rela_t *rela = (elf32_rela_t *)rela_vaddr;
    u32 n = rela_sz / sizeof(elf32_rela_t);
    (void)plt;

    for (u32 i = 0; i < n; i++) {
        u32 type = rela[i].r_info & 0xFF;
        u32 sym  = rela[i].r_info >> 8;
        u32 *where = (u32 *)(rela[i].r_offset + bias);

        switch (type) {
        case R_386_NONE:
            break;
        case R_386_RELATIVE:
            *where = rela[i].r_addend + bias;
            break;
        case R_386_GLOB_DAT:
        case R_386_JUMP_SLOT: {
            lib_t *lib;
            u32 sym_val = 0;
            for (lib = loaded_libs; lib; lib = lib->next) {
                if (!lib->symtab || !lib->strtab) continue;
                u32 nsyms = lib->symtabsz >= 16 ? lib->symtabsz / 16 : 0;
                elf32_sym_t *syms = (elf32_sym_t *)lib->symtab;
                const char *strs = (const char *)lib->strtab;
                if (sym < nsyms && syms[sym].st_name) {
                    const char *sname = strs + syms[sym].st_name;
                    sym_val = find_sym(sname);
                    break;
                }
            }
            *where = sym_val ? sym_val : 0;
            break;
        }
        case R_386_32: {
            u32 sym_val = 0;
            lib_t *lib;
            for (lib = loaded_libs; lib; lib = lib->next) {
                if (!lib->symtab || !lib->strtab) continue;
                u32 nsyms = lib->symtabsz >= 16 ? lib->symtabsz / 16 : 0;
                elf32_sym_t *syms = (elf32_sym_t *)lib->symtab;
                const char *strs = (const char *)lib->strtab;
                if (sym < nsyms && syms[sym].st_name) {
                    const char *sname = strs + syms[sym].st_name;
                    sym_val = find_sym(sname);
                    break;
                }
            }
            *where += sym_val;
            break;
        }
        case R_386_PC32: {
            u32 sym_val = 0;
            lib_t *lib;
            for (lib = loaded_libs; lib; lib = lib->next) {
                if (!lib->symtab || !lib->strtab) continue;
                u32 nsyms = lib->symtabsz >= 16 ? lib->symtabsz / 16 : 0;
                elf32_sym_t *syms = (elf32_sym_t *)lib->symtab;
                const char *strs = (const char *)lib->strtab;
                if (sym < nsyms && syms[sym].st_name) {
                    const char *sname = strs + syms[sym].st_name;
                    sym_val = find_sym(sname);
                    break;
                }
            }
            *where += sym_val - (u32)(unsigned long)where;
            break;
        }
        default:
            break;
        }
    }
}

/* Process RELR (compact relative) relocations. */
static void process_relr(u32 relr_vaddr, u32 relr_sz, u32 bias)
{
    u32 *relr = (u32 *)relr_vaddr;
    u32 n = relr_sz / 4;
    u32 where = 0;

    for (u32 i = 0; i < n; i++) {
        u32 entry = relr[i];
        if ((entry & 1) == 0) {
            where = entry + bias;
            *(u32 *)where += bias;
        } else {
            u32 bitmap = entry;
            where += 4;
            for (int b = 0; b < 31; b++) {
                if (bitmap & (2u << b)) {
                    *(u32 *)(where + b * 4) += bias;
                }
            }
            where += 31 * 4;
        }
    }
}

/* Apply relocations for a library. Called after all dependencies are loaded. */
static void relocate_lib(lib_t *lib)
{
    if (!lib->dyn_vaddr) return;
    elf32_dyn_t *dyn = (elf32_dyn_t *)lib->dyn_vaddr;

    u32 rela_vaddr = 0, rela_sz = 0;
    u32 relr_vaddr = 0, relr_sz = 0;
    u32 pltrel_vaddr = 0, pltrel_sz = 0;
    u32 pltrel_type = 0;
    u32 jmprel_vaddr = 0, jmprel_sz = 0;

    for (; dyn->d_tag != 0; dyn++) {
        switch (dyn->d_tag) {
        case DT_RELA:     rela_vaddr = dyn->d_val + lib->base; break;
        case DT_RELASZ:   rela_sz = dyn->d_val; break;
        case DT_RELR:     relr_vaddr = dyn->d_val + lib->base; break;
        case DT_RELRSZ:   relr_sz = dyn->d_val; break;
        case DT_PLTREL:   pltrel_type = dyn->d_val; break;
        case DT_JMPREL:   jmprel_vaddr = dyn->d_val + lib->base; break;
        case DT_PLTRELSZ: jmprel_sz = dyn->d_val; break;
        }
    }

    if (rela_vaddr && rela_sz)
        process_rela(rela_vaddr, rela_sz, lib->base, 0);

    if (relr_vaddr && relr_sz)
        process_relr(relr_vaddr, relr_sz, lib->base);

    if (jmprel_vaddr && jmprel_sz) {
        if (pltrel_type == DT_RELA) {
            process_rela(jmprel_vaddr, jmprel_sz, lib->base, 1);
        }
    }

    /* Make RELRO region read-only */
    if (lib->relro_start && lib->relro_end) {
        u32 start = lib->relro_start & 0xFFFFF000;
        u32 end = align_up(lib->relro_end, 4096);
        munmap((void *)start, end - start);
        mmap((void *)start, end - start, PROT_READ, MAP_PRIVATE | MAP_FIXED, -1, 0);
    }
}

/* Call init functions for a library. */
static void call_init(lib_t *lib)
{
    if (lib->init) {
        void (*f)(void) = (void (*)(void))lib->init;
        f();
    }
    if (lib->init_array && lib->init_arraysz) {
        u32 n = lib->init_arraysz / 4;
        void (**arr)(void) = (void (**)(void))lib->init_array;
        for (u32 i = 0; i < n; i++) {
            arr[i]();
        }
    }
}

/* Call fini functions for a library (in reverse order). */
static void call_fini(lib_t *lib)
{
    if (lib->fini_array && lib->fini_arraysz) {
        u32 n = lib->fini_arraysz / 4;
        void (**arr)(void) = (void (**)(void))lib->fini_array;
        for (u32 i = n; i > 0; i--) {
            arr[i - 1]();
        }
    }
    if (lib->fini) {
        void (*f)(void) = (void (*)(void))lib->fini;
        f();
    }
}

/* Load all DT_NEEDED libraries for the main program (or a loaded lib). */
static void load_needed(u32 dyn_vaddr, u32 bias)
{
    if (!dyn_vaddr) return;
    elf32_dyn_t *dyn = (elf32_dyn_t *)dyn_vaddr;
    u32 strtab = 0, strtabsz = 0;

    /* First pass: find strtab */
    for (; dyn->d_tag != 0; dyn++) {
        if (dyn->d_tag == DT_STRTAB) strtab = dyn->d_val + bias;
        if (dyn->d_tag == DT_STRSZ)  strtabsz = dyn->d_val;
    }

    if (!strtab) return;
    const char *strings = (const char *)strtab;

    /* Second pass: load each NEEDED library */
    dyn = (elf32_dyn_t *)dyn_vaddr;
    for (; dyn->d_tag != 0; dyn++) {
        if (dyn->d_tag == DT_NEEDED) {
            const char *libname = strings + dyn->d_val;
            /* Skip if already loaded */
            int already = 0;
            for (lib_t *l = loaded_libs; l; l = l->next) {
                (void)l;
                /* We don't store names; just try to load and if it fails, skip */
            }
            (void)already;
            /* Search in standard paths */
            char fullpath[256];
            const char *paths[] = {"/usr/lib/", "/lib/", NULL};
            int loaded = 0;
            for (int p = 0; paths[p]; p++) {
                unsigned int pi = 0;
                while (paths[p][pi]) { fullpath[pi] = paths[p][pi]; pi++; }
                unsigned int li = 0;
                while (libname[li] && pi < sizeof(fullpath) - 1) { fullpath[pi++] = libname[li++]; }
                fullpath[pi] = '\0';
                lib_t *nl = load_library(fullpath);
                if (nl) {
                    loaded = 1;
                    break;
                }
            }
            if (!loaded) {
                /* Try just the name as a path */
                load_library(libname);
            }
        }
    }
}

/* The entry point.
 * Stack layout on entry:
 *   [esp]   = argc
 *   [esp+4] = argv[0..argc-1], NULL
 *   [esp+...] = envp[0..], NULL
 *   [esp+...] = auxv[0..AT_NULL]
 */
void __attribute__((section(".text"))) _start(void)
{
    u32 *stack;
    asm volatile("mov %%esp, %0" : "=r"(stack));

    int argc = (int)stack[0];
    char **argv = (char **)(stack + 1);
    char **envp = (char **)(stack + 1 + argc + 1);

    /* Scan auxv */
    u32 phdr = 0, phnum = 0, interp_base = 0, user_entry = 0, pagesz = 4096;
    char **av = envp;
    while (*av) av++;
    u32 *auxv = (u32 *)(av + 1);
    for (int i = 0; auxv[i] != AT_NULL; i += 2) {
        switch (auxv[i]) {
        case AT_PHDR:   phdr = auxv[i+1]; break;
        case AT_PHNUM:  phnum = auxv[i+1]; break;
        case AT_BASE:   interp_base = auxv[i+1]; break;
        case AT_ENTRY:  user_entry = auxv[i+1]; break;
        case AT_PAGESZ: pagesz = auxv[i+1]; break;
        }
    }

    if (!phdr || !phnum || !user_entry)
        exit(1);

    /* Find PT_DYNAMIC in the main program */
    elf32_phdr_t *main_phdrs = (elf32_phdr_t *)phdr;
    u32 main_dyn = 0;
    for (u32 i = 0; i < phnum; i++) {
        if (main_phdrs[i].type == PT_DYNAMIC) {
            main_dyn = main_phdrs[i].vaddr;
            break;
        }
    }

    /* Load needed libraries */
    load_needed(main_dyn, 0);

    /* Now load NEEDED for each loaded library (transitive deps) */
    for (lib_t *lib = loaded_libs; lib; lib = lib->next) {
        load_needed(lib->dyn_vaddr, lib->base);
    }

    /* Relocate libraries in reverse order (dependencies first) */
    /* Simple approach: just relocate all loaded libs */
    for (lib_t *lib = loaded_libs; lib; lib = lib->next) {
        relocate_lib(lib);
    }

    /* Relocate main program (if it has dynamic entries) */
    if (main_dyn) {
        lib_t main_lib;
        main_lib.base = 0;
        main_lib.dyn_vaddr = main_dyn;
        main_lib.relro_start = 0;
        main_lib.relro_end = 0;
        /* We need to handle main program's RELRO too */
        for (u32 i = 0; i < phnum; i++) {
            if (main_phdrs[i].type == PT_GNU_RELRO) {
                main_lib.relro_start = main_phdrs[i].vaddr;
                main_lib.relro_end = main_phdrs[i].vaddr + main_phdrs[i].memsz;
            }
        }
        relocate_lib(&main_lib);
    }

    /* Call init functions in dependency order: libraries first, then main */
    for (lib_t *lib = loaded_libs; lib; lib = lib->next) {
        call_init(lib);
    }
    /* Main program's init */
    if (main_dyn) {
        elf32_dyn_t *dyn = (elf32_dyn_t *)main_dyn;
        u32 init = 0, init_array = 0, init_arraysz = 0;
        for (; dyn->d_tag != 0; dyn++) {
            switch (dyn->d_tag) {
            case DT_INIT:       init = dyn->d_val; break;
            case DT_INIT_ARRAY:   init_array = dyn->d_val; break;
            case DT_INIT_ARRAYSZ: init_arraysz = dyn->d_val; break;
            }
        }
        if (init) {
            void (*f)(void) = (void (*)(void))init;
            f();
        }
        if (init_array && init_arraysz) {
            u32 n = init_arraysz / 4;
            void (**arr)(void) = (void (**)(void))init_array;
            for (u32 i = 0; i < n; i++) {
                arr[i]();
            }
        }
    }

    /* Jump to the main program's entry point.
     * Pass argc, argv, envp as if nothing happened. */
    asm volatile(
        "mov %0, %%esp\n\t"
        "mov %1, %%eax\n\t"
        "jmp *%2\n\t"
        :
        : "r"(stack), "r"((u32)argc), "r"(user_entry)
        : "memory"
    );
    __builtin_unreachable();
}
