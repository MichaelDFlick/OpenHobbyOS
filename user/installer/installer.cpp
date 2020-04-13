#define __bool_true_false_are_defined 1

#include "syscall.h"
#include "runtime.h"

struct MbrPart {
    unsigned char status;
    unsigned char chs_start[3];
    unsigned char type;
    unsigned char chs_end[3];
    unsigned int lba_start;
    unsigned int sector_count;
} __attribute__((packed));

struct Installer {
    int probe_disks(unsigned int *ids, unsigned int max_count) {
        return sys_install_op(0, 0, 0, ids, max_count * sizeof(unsigned int));
    }

    unsigned long long disk_size(unsigned int dev_id) {
        unsigned long long sz = 0;
        sys_install_op(1, dev_id, 0, &sz, 8);
        return sz;
    }

    int mbr_write(unsigned int dev_id, MbrPart *parts) {
        return sys_install_op(2, dev_id, 0, parts, 4 * sizeof(MbrPart));
    }

    int format_ext2(unsigned int dev_id, unsigned int start, unsigned int sectors) {
        return sys_install_op(3, dev_id, start, 0, sectors);
    }

    int mkdir_ext2(unsigned int dev_id, unsigned int start, const char *path) {
        unsigned int len = 0;
        const char *p = path;
        while (*p) { len++; p++; }
        return sys_install_op(5, dev_id, start, (void *)path, len);
    }

    int copy_initrd(unsigned int dev_id, unsigned int start) {
        return sys_install_op(6, dev_id, start, 0, 0);
    }

    void reboot(void) {
        u_puts("Rebooting...\n");
        sys_reboot();
    }

    void clear_screen(void) {
        sys_write(1, "\033[2J\033[H", 7);
    }

    void read_line(char *buf, unsigned int size) {
        unsigned int pos = 0;
        while (pos + 1 < size) {
            char ch;
            if (sys_read(0, &ch, 1) != 1) break;
            if (ch == '\n') break;
            if (ch == '\r') break;
            if (ch == '\b' || ch == 127) {
                if (pos > 0) {
                    pos--;
                    sys_write(1, "\b \b", 3);
                }
                continue;
            }
            buf[pos++] = ch;
            sys_write(1, &ch, 1);
        }
        buf[pos] = '\0';
        sys_write(1, "\n", 1);
    }

    int get_key(void) {
        char ch = 0;
        while (sys_read(0, &ch, 1) != 1) {}
        return (int)(unsigned char)ch;
    }

    void show_main_menu(void) {
        clear_screen();
        u_puts("OpenHobbyOS Installer\n");
        u_puts("=====================\n\n");
        u_puts("1.  Live Boot  -  Boot from CD without installing\n");
        u_puts("2.  Install to Disk  -  Install OpenHobbyOS to hard disk\n");
        u_puts("3.  Reboot\n\n");
        u_puts("Enter choice [1-3]: ");
    }

    void live_boot(void) {
        clear_screen();
        u_puts("Starting Live Boot...\n\n");
        const char *argv[] = {"/bin/gosh", 0};
        sys_execve("/bin/gosh", (char *const *)argv, 0);
        u_puts("Failed to start gosh!\n");
    }

    int confirm_destructive(void) {
        clear_screen();
        sys_write(1, "\033[1;31m", 7);
        u_puts("!!! WARNING !!!\n");
        sys_write(1, "\033[0m", 4);
        u_puts("\nThis will ERASE ALL DATA on the selected disk.\n");
        u_puts("Make sure you have backups of any important data.\n\n");
        u_puts("Type 'yes' to continue: ");
        char buf[8];
        read_line(buf, sizeof(buf));
        return u_strcmp(buf, "yes") == 0;
    }

    int install_to_disk(void) {
        if (!confirm_destructive()) {
            u_puts("\nInstallation cancelled.\n");
            return -1;
        }

        unsigned int disk_ids[4];
        int disk_count = probe_disks(disk_ids, 4);
        if (disk_count <= 0) {
            u_puts("\nNo disks found! Cannot install.\n");
            return -1;
        }

        unsigned int dev_id = disk_ids[0];
        unsigned long long total_sectors = disk_size(dev_id);
        u_puts("\nTarget disk: ");
        u_put_uint(dev_id);
        u_puts(" (");
        u_put_u64(total_sectors / 2048u);
        u_puts(" MB)\n\n");

        u_puts("Proceed with installation? [y/N]: ");
        char ch = (char)get_key();
        sys_write(1, &ch, 1);
        sys_write(1, "\n", 1);
        if (ch != 'y' && ch != 'Y') {
            u_puts("Installation cancelled.\n");
            return -1;
        }

        unsigned int start_sector = 2048;
        unsigned int part_sectors = (unsigned int)(total_sectors - start_sector);

        MbrPart parts[4];
        for (int i = 0; i < 4; i++) {
            MbrPart *p = &parts[i];
            p->status = 0;
            p->chs_start[0] = 0; p->chs_start[1] = 0; p->chs_start[2] = 0;
            p->type = 0;
            p->chs_end[0] = 0; p->chs_end[1] = 0; p->chs_end[2] = 0;
            p->lba_start = 0;
            p->sector_count = 0;
        }

        parts[0].status = 0x80;
        parts[0].type = 0x83;
        parts[0].lba_start = start_sector;
        parts[0].sector_count = part_sectors;

        u_puts("Partitioning... ");
        if (mbr_write(dev_id, parts) != 0) {
            u_puts("FAILED!\n");
            return -1;
        }
        u_puts("OK\n");

        u_puts("Formatting ext2... ");
        if (format_ext2(dev_id, start_sector, part_sectors) != 0) {
            u_puts("FAILED!\n");
            return -1;
        }
        u_puts("OK\n");

        u_puts("Copying system files... ");
        int copied = copy_initrd(dev_id, start_sector);
        if (copied < 0) {
            u_puts("FAILED!\n");
            return -1;
        }
        u_puts("OK (");
        u_put_int(copied);
        u_puts(" files)\n");

        if (mkdir_ext2(dev_id, start_sector, "/home") != 0) {
            u_puts("Warning: could not create /home\n");
        }

        u_puts("\nCreate a user account:\n");
        u_puts("Username [user]: ");
        char username[32];
        read_line(username, sizeof(username));
        if (username[0] == '\0') {
            u_strcpy(username, "user");
        }

        char home_path[64];
        u_strcpy(home_path, "/home/");
        unsigned int off = 6;
        for (unsigned int i = 0; username[i] && off + 1 < sizeof(home_path); i++)
            home_path[off++] = username[i];
        home_path[off] = '\0';

        u_puts("Creating home directory ");
        u_puts(home_path);
        u_puts("... ");
        if (mkdir_ext2(dev_id, start_sector, home_path) != 0) {
            u_puts("Warning: could not create home dir\n");
        } else {
            u_puts("OK\n");
        }

        clear_screen();
        sys_write(1, "\033[1;32m", 7);
        u_puts("Installation Complete!\n");
        sys_write(1, "\033[0m", 4);
        u_puts("\n");
        u_puts("OpenHobbyOS has been installed to the disk.\n");
        u_puts("Remove the installation media and reboot.\n");
        u_puts("User account: ");
        u_puts(username);
        u_puts("\n\n");
        u_puts("1.  Reboot\n");
        u_puts("2.  Return to main menu\n\n");
        u_puts("Enter choice [1-2]: ");

        char choice[4];
        read_line(choice, sizeof(choice));
        if (choice[0] == '1') {
            reboot();
        }
        return 0;
    }
};

extern "C" int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;
    Installer installer;
    for (;;) {
        installer.show_main_menu();
        char buf[4];
        installer.read_line(buf, sizeof(buf));
        if (buf[0] == '1') {
            installer.live_boot();
        } else if (buf[0] == '2') {
            installer.install_to_disk();
        } else if (buf[0] == '3') {
            installer.clear_screen();
            installer.reboot();
        }
    }
    sys_exit(0);
    return 0;
}
