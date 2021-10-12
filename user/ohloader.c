#include "runtime.h"
#include "syscall.h"

#define OHLOADER_ARGS_MAX 64
#define OHLOADER_BUF_SIZE 4096

static int ohloader_stop;

static unsigned long parse_ulong(const char *s) {
    unsigned long v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned long)(*s++ - '0');
    }
    return v;
}

static void ohloader_help(void) {
    u_puts("Usage: ohloader [mode] [options]\n");
    u_puts("Modes: cpu, mem, io, syscall, all (default)\n");
    u_puts("Options: -t N (seconds, 0=forever, default 10)\n");
    u_puts("         -w N (workers, default 2)\n");
    u_puts("         -s N (mem size KB, default 512)\n");
}

static unsigned long now_sec(void) {
    struct linux_timespec ts;
    if (sys_clock_gettime(1, &ts) == 0) {
        return (unsigned long)ts.tv_sec;
    }
    return 0;
}

static int cpu_stress(unsigned long duration) {
    unsigned long start = now_sec();
    while (!ohloader_stop) {
        if (duration && now_sec() - start >= duration) break;
        volatile unsigned long x = 0;
        for (unsigned long i = 0; i < 50000; i++) {
            x += i * i;
            x ^= i;
            x = (x << 3) | (x >> 29);
        }
        sys_sched_yield();
    }
    return 0;
}

static int mem_stress(unsigned long duration, unsigned long size_kb) {
    unsigned int page_count = (unsigned int)(size_kb / 4);
    if (page_count < 1) page_count = 1;
    unsigned long start = now_sec();

    while (!ohloader_stop) {
        if (duration && now_sec() - start >= duration) break;

        unsigned int ptr_bytes = (page_count * sizeof(void*) + 4095u) & ~4095u;
        void **pages = (void **)sys_mmap2(0, ptr_bytes, 3, 0x22, -1, 0);
        if (!pages || (long)pages < 0) {
            return 1;
        }

        unsigned int allocated = 0;
        for (unsigned int i = 0; i < page_count; i++) {
            void *p = sys_mmap2(0, 4096, 3, 0x22, -1, 0);
            if (!p || (long)p < 0) break;
            pages[allocated++] = p;
            unsigned char *ptr = (unsigned char *)p;
            for (int j = 0; j < 4096; j += 64) {
                ptr[j] = (unsigned char)(i + j);
            }
        }

        for (unsigned int i = 0; i < allocated; i++) {
            sys_munmap(pages[i], 4096);
        }
        sys_munmap(pages, ptr_bytes);
    }
    return 0;
}

static int io_stress(unsigned long duration) {
    unsigned long start = now_sec();

    while (!ohloader_stop) {
        if (duration && now_sec() - start >= duration) break;

        int fd = sys_open("/ohloader_test", 0x42, 0666);
        if (fd < 0) {
            return 1;
        }

        unsigned char buf[OHLOADER_BUF_SIZE];
        int written = sys_write(fd, buf, sizeof(buf));
        if (written < 0) {
            sys_close(fd);
            return 1;
        }

        if (sys_lseek(fd, 0, 0) < 0) {
            sys_close(fd);
            return 1;
        }

        unsigned char read_buf[OHLOADER_BUF_SIZE];
        sys_read(fd, read_buf, sizeof(read_buf));
        sys_close(fd);
        sys_unlink("/ohloader_test");
    }

    return 0;
}

static int syscall_stress(unsigned long duration) {
    unsigned long start = now_sec();

    while (!ohloader_stop) {
        if (duration && now_sec() - start >= duration) break;

        for (int i = 0; i < 1000; i++) {
            sys_getpid();
            sys_getuid32();
            {
                struct linux_timespec ts;
                sys_clock_gettime(1, &ts);
            }
            sys_sched_yield();
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    ohloader_stop = 0;

    const char *mode = "all";
    unsigned long duration = 10;
    unsigned int workers = 2;
    unsigned long mem_kb = 512;

    for (int i = 1; i < argc && i < OHLOADER_ARGS_MAX; i++) {
        const char *a = argv[i];
        if (a[0] == '-') {
            if (a[1] == 'h') {
                ohloader_help();
                return 0;
            }
            if (a[1] == 't' && i + 1 < argc) {
                duration = parse_ulong(argv[++i]);
            } else if (a[1] == 'w' && i + 1 < argc) {
                workers = (unsigned int)parse_ulong(argv[++i]);
                if (workers < 1) workers = 1;
            } else if (a[1] == 's' && i + 1 < argc) {
                mem_kb = parse_ulong(argv[++i]);
            }
        } else {
            mode = a;
        }
    }

    int do_cpu = 0, do_mem = 0, do_io = 0, do_syscall = 0;
    if (u_strcmp(mode, "cpu") == 0) do_cpu = 1;
    else if (u_strcmp(mode, "mem") == 0) do_mem = 1;
    else if (u_strcmp(mode, "io") == 0) do_io = 1;
    else if (u_strcmp(mode, "syscall") == 0) do_syscall = 1;
    else if (u_strcmp(mode, "all") == 0) {
        do_cpu = 1; do_mem = 1; do_io = 1; do_syscall = 1;
    } else {
        return 1;
    }

    if (do_cpu) {
        for (unsigned int i = 0; i < workers; i++) {
            int pid = sys_fork();
            if (pid < 0) break;
            if (pid == 0) {
                cpu_stress(duration);
                sys_exit(0);
                return 0;
            }
        }
    }

    if (do_io) {
        for (unsigned int i = 0; i < workers; i++) {
            int pid = sys_fork();
            if (pid < 0) break;
            if (pid == 0) {
                io_stress(duration);
                sys_exit(0);
                return 0;
            }
        }
    }

    if (do_syscall) {
        for (unsigned int i = 0; i < workers; i++) {
            int pid = sys_fork();
            if (pid < 0) break;
            if (pid == 0) {
                syscall_stress(duration);
                sys_exit(0);
                return 0;
            }
        }
    }

    if (do_mem) {
        mem_stress(duration, mem_kb);
    }

    if (do_cpu || do_io || do_syscall) {
        int status = 0;
        for (;;) {
            int ret = sys_waitpid(-1, &status, 0);
            if (ret <= 0) break;
        }
    }

    return 0;
}
