#include "syscall.h"
#include "abi/linux.h"

#define MAX_USER 32
#define MAX_PASS 64
#define MAX_ATTEMPTS 3

static int read_char(void) {
    unsigned char ch;
    int n = sys_read(0, &ch, 1);
    if (n != 1) return -1;
    return ch;
}

static void write_str(const char *s) {
    int len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

static void write_ch(char c) {
    sys_write(1, &c, 1);
}

static int read_username(char *buf, int max) {
    int pos = 0;
    for (;;) {
        int ch = read_char();
        if (ch < 0) return -1;
        if (ch == '\n' || ch == '\r') {
            write_str("\r\n");
            buf[pos] = '\0';
            return pos;
        }
        if (ch == '\b' || ch == 127) {
            if (pos > 0) {
                pos--;
                write_str("\b \b");
            }
            continue;
        }
        if (pos < max - 1) {
            buf[pos++] = (char)ch;
            write_ch((char)ch);
        }
    }
}

static int read_password(char *buf, int max) {
    int pos = 0;
    for (;;) {
        int ch = read_char();
        if (ch < 0) return -1;
        if (ch == '\n' || ch == '\r') {
            write_str("\r\n");
            buf[pos] = '\0';
            return pos;
        }
        if (ch == '\b' || ch == 127) {
            if (pos > 0) {
                pos--;
                write_str("\b \b");
            }
            continue;
        }
        if (pos < max - 1) {
            buf[pos++] = (char)ch;
            write_str("\b \b");
            write_ch('*');
        }
    }
}

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    for (;;) {
        sys_write(1, "\033[H\033[J", 5);
        write_str("=================================\n");
        write_str("       OpenHobbyOS v0.1.0\n");
        write_str("=================================\n\n");

        char username[MAX_USER];
        char password[MAX_PASS];

        write_str("login: ");
        if (read_username(username, MAX_USER) < 0) continue;

        write_str("password: ");
        if (read_password(password, MAX_PASS) < 0) continue;

        int ret = sys_auth(username, password);
        for (int i = 0; password[i]; i++) password[i] = '\0';

        if (ret == 0) {
            write_str("\n");
            const char *shell_argv[] = {"/bin/sh", NULL};
            const char *shell_envp[] = {"PATH=/bin:/usr/bin", "HOME=/root", NULL};
            sys_execve("/bin/sh", (char **)shell_argv, (char **)shell_envp);
            return 0;
        }

        write_str("Login incorrect\n\n");
        for (int i = 0; i < 10000000; i++) {
            __asm__ volatile ("pause");
        }
    }
}
