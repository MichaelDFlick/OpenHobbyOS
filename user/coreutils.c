#include "syscall.h"
#include "abi/linux.h"

#define BUF_SIZE 32768

static int read_all(int fd, char *buf, int size) {
    int total = 0;
    while (total < size) {
        int n = sys_read(fd, buf + total, size - total);
        if (n <= 0) break;
        total += n;
    }
    return total;
}

static int write_str(int fd, const char *s) {
    int len = 0;
    while (s[len]) len++;
    return sys_write(fd, s, len);
}

static int write_buf(int fd, const char *buf, int len) {
    return sys_write(fd, buf, len);
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int str_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char *)a - *(unsigned char *)b;
}

static int str_ncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) break;
    }
    return 0;
}

static char *find_char(const char *s, int c) {
    while (*s) { if (*s == c) return (char *)s; s++; }
    return 0;
}

static char *rfind_char(const char *s, int c) {
    const char *p = 0;
    while (*s) { if (*s == c) p = s; s++; }
    return (char *)p;
}

static int is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int is_digit(int c) {
    return c >= '0' && c <= '9';
}

static int parse_int(const char **s) {
    int n = 0, sign = 1;
    while (is_space(**s)) (*s)++;
    if (**s == '-') { sign = -1; (*s)++; }
    while (is_digit(**s)) { n = n * 10 + (**s - '0'); (*s)++; }
    return n * sign;
}

static int cp_cmd(int argc, char **argv) {
    if (argc < 3) { write_str(2, "cp: missing operand\n"); return 1; }
    int src_fd = sys_open(argv[1], LINUX_O_RDONLY, 0);
    if (src_fd < 0) { write_str(2, "cp: cannot open "); write_str(2, argv[1]); write_str(2, "\n"); return 1; }
    int dst_fd = sys_open(argv[2], LINUX_O_WRONLY | LINUX_O_CREAT | LINUX_O_TRUNC, 0644);
    if (dst_fd < 0) { sys_close(src_fd); write_str(2, "cp: cannot create "); write_str(2, argv[2]); write_str(2, "\n"); return 1; }
    char buf[BUF_SIZE];
    int n;
    while ((n = sys_read(src_fd, buf, BUF_SIZE)) > 0) sys_write(dst_fd, buf, n);
    sys_close(src_fd); sys_close(dst_fd);
    return 0;
}

static int mv_cmd(int argc, char **argv) {
    if (argc < 3) { write_str(2, "mv: missing operand\n"); return 1; }
    if (sys_rename(argv[1], argv[2]) < 0) {
        write_str(2, "mv: cannot rename "); write_str(2, argv[1]); write_str(2, "\n"); return 1;
    }
    return 0;
}

static int rm_cmd(int argc, char **argv) {
    if (argc < 2) { write_str(2, "rm: missing operand\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        if (sys_unlink(argv[i]) < 0) {
            write_str(2, "rm: cannot remove "); write_str(2, argv[i]); write_str(2, "\n");
        }
    }
    return 0;
}

static int ln_cmd(int argc, char **argv) {
    if (argc < 3) { write_str(2, "ln: missing operand\n"); return 1; }
    if (sys_link(argv[1], argv[2]) < 0) {
        write_str(2, "ln: cannot link "); write_str(2, argv[1]); write_str(2, "\n"); return 1;
    }
    return 0;
}

static int chmod_cmd(int argc, char **argv) {
    if (argc < 3) { write_str(2, "chmod: missing operand\n"); return 1; }
    int mode = 0;
    const char *m = argv[1];
    if (str_len(m) == 3) {
        for (int i = 0; i < 3; i++) {
            if (m[i] >= '0' && m[i] <= '7') mode = (mode << 3) | (m[i] - '0');
        }
    }
    if (mode == 0) mode = 0644;
    for (int i = 2; i < argc; i++) {
        if (sys_chmod(argv[i], mode) < 0) {
            write_str(2, "chmod: cannot change "); write_str(2, argv[i]); write_str(2, "\n");
        }
    }
    return 0;
}

static int touch_cmd(int argc, char **argv) {
    if (argc < 2) { write_str(2, "touch: missing operand\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        int fd = sys_open(argv[i], LINUX_O_WRONLY | LINUX_O_CREAT, 0644);
        if (fd < 0) {
            write_str(2, "touch: cannot create "); write_str(2, argv[i]); write_str(2, "\n");
        } else {
            sys_close(fd);
        }
    }
    return 0;
}

static int head_cmd(int argc, char **argv) {
    int n = 10, argi = 1;
    if (argc > 1 && argv[1][0] == '-' && is_digit(argv[1][1])) {
        n = parse_int((const char **)&argv[1]);
        argi = 2;
    }
    int fd = (argi < argc) ? sys_open(argv[argi], LINUX_O_RDONLY, 0) : 0;
    if (fd < 0) { write_str(2, "head: cannot open\n"); return 1; }
    char buf[1];
    int lines = 0;
    while (lines < n && sys_read(fd, buf, 1) > 0) {
        sys_write(1, buf, 1);
        if (buf[0] == '\n') lines++;
    }
    if (fd != 0) sys_close(fd);
    return 0;
}

static int tail_cmd(int argc, char **argv) {
    int n = 10, argi = 1;
    if (argc > 1 && argv[1][0] == '-' && is_digit(argv[1][1])) {
        n = parse_int((const char **)&argv[1]);
        argi = 2;
    }
    int fd = (argi < argc) ? sys_open(argv[argi], LINUX_O_RDONLY, 0) : 0;
    if (fd < 0) { write_str(2, "tail: cannot open\n"); return 1; }
    char buf[BUF_SIZE];
    int total = read_all(fd, buf, BUF_SIZE);
    if (fd != 0) sys_close(fd);
    int lines = 0;
    for (int i = total - 1; i >= 0; i--) {
        if (buf[i] == '\n') lines++;
        if (lines > n) { write_buf(1, buf + i + 1, total - i - 1); return 0; }
    }
    write_buf(1, buf, total);
    return 0;
}

static int wc_cmd(int argc, char **argv) {
    int fd = (argc > 1) ? sys_open(argv[1], LINUX_O_RDONLY, 0) : 0;
    if (fd < 0) { write_str(2, "wc: cannot open\n"); return 1; }
    char buf[BUF_SIZE];
    int total = read_all(fd, buf, BUF_SIZE);
    if (fd != 0) sys_close(fd);
    int lines = 0, words = 0, in_word = 0;
    for (int i = 0; i < total; i++) {
        if (buf[i] == '\n') lines++;
        if (is_space(buf[i])) { in_word = 0; }
        else if (!in_word) { in_word = 1; words++; }
    }
    char num[64];
    int pos = 0;
    int n = lines;
    do { num[pos++] = '0' + n % 10; n /= 10; } while (n);
    while (pos > 0) write_buf(1, &num[--pos], 1);
    write_str(1, " ");
    pos = 0; n = words;
    do { num[pos++] = '0' + n % 10; n /= 10; } while (n);
    while (pos > 0) write_buf(1, &num[--pos], 1);
    write_str(1, " ");
    pos = 0; n = total;
    do { num[pos++] = '0' + n % 10; n /= 10; } while (n);
    while (pos > 0) write_buf(1, &num[--pos], 1);
    write_str(1, " ");
    if (argc > 1) write_str(1, argv[1]);
    write_str(1, "\n");
    return 0;
}

static int sort_cmd(int argc, char **argv) {
    int fd = (argc > 1) ? sys_open(argv[1], LINUX_O_RDONLY, 0) : 0;
    if (fd < 0) { write_str(2, "sort: cannot open\n"); return 1; }
    char all[BUF_SIZE];
    int total = read_all(fd, all, BUF_SIZE);
    if (fd != 0) sys_close(fd);
    char *lines[4096];
    int nlines = 0;
    lines[nlines++] = all;
    for (int i = 0; i < total && nlines < 4096; i++) {
        if (all[i] == '\n') {
            all[i] = '\0';
            if (i + 1 < total) lines[nlines++] = all + i + 1;
        }
    }
    for (int i = 0; i < nlines - 1; i++) {
        for (int j = i + 1; j < nlines; j++) {
            if (str_cmp(lines[i], lines[j]) > 0) {
                char *tmp = lines[i]; lines[i] = lines[j]; lines[j] = tmp;
            }
        }
    }
    for (int i = 0; i < nlines; i++) {
        write_str(1, lines[i]);
        write_str(1, "\n");
    }
    return 0;
}

static int uniq_cmd(int argc, char **argv) {
    int fd = (argc > 1) ? sys_open(argv[1], LINUX_O_RDONLY, 0) : 0;
    if (fd < 0) { write_str(2, "uniq: cannot open\n"); return 1; }
    char all[BUF_SIZE];
    int total = read_all(fd, all, BUF_SIZE);
    if (fd != 0) sys_close(fd);
    char *prev = 0;
    char *lines[4096];
    int nlines = 0;
    lines[nlines++] = all;
    for (int i = 0; i < total && nlines < 4096; i++) {
        if (all[i] == '\n') {
            all[i] = '\0';
            if (i + 1 < total) lines[nlines++] = all + i + 1;
        }
    }
    for (int i = 0; i < nlines; i++) {
        if (!prev || str_cmp(lines[i], prev) != 0) {
            write_str(1, lines[i]);
            write_str(1, "\n");
        }
        prev = lines[i];
    }
    return 0;
}

static int cut_cmd(int argc, char **argv) {
    int fd = 0, delim = '\t', field = 1;
    for (int i = 1; i < argc; i++) {
        if (str_ncmp(argv[i], "-d", 2) == 0 && argv[i][2]) { delim = argv[i][2]; }
        else if (str_ncmp(argv[i], "-f", 2) == 0 && argv[i][2]) { field = parse_int((const char **)&argv[i][2]); }
        else { fd = sys_open(argv[i], LINUX_O_RDONLY, 0); if (fd < 0) { write_str(2, "cut: cannot open\n"); return 1; } }
    }
    char buf[BUF_SIZE];
    int total = read_all(fd, buf, BUF_SIZE);
    if (fd != 0) sys_close(fd);
    int line_start = 0;
    for (int i = 0; i <= total; i++) {
        if (i == total || buf[i] == '\n') {
            int f = 1, start = line_start;
            for (int j = line_start; j < i; j++) {
                if (buf[j] == delim) { f++; if (f > field) { write_buf(1, buf + start, j - start); write_str(1, "\n"); break; } start = j + 1; }
            }
            if (f <= field) { write_buf(1, buf + start, i - start); write_str(1, "\n"); }
            line_start = i + 1;
        }
    }
    return 0;
}

static int tr_cmd(int argc, char **argv) {
    if (argc < 3) { write_str(2, "tr: missing operand\n"); return 1; }
    const char *from = argv[1], *to = argv[2];
    char buf[BUF_SIZE];
    int total = read_all(0, buf, BUF_SIZE);
    for (int i = 0; i < total; i++) {
        const char *p = find_char(from, (unsigned char)buf[i]);
        if (p && (p - from) < str_len(to)) buf[i] = to[p - from];
    }
    write_buf(1, buf, total);
    return 0;
}

static int tee_cmd(int argc, char **argv) {
    int fds[16], nfds = 0;
    for (int i = 1; i < argc && nfds < 16; i++) {
        int fd = sys_open(argv[i], LINUX_O_WRONLY | LINUX_O_CREAT | LINUX_O_TRUNC, 0644);
        if (fd >= 0) fds[nfds++] = fd;
    }
    char buf[1];
    while (sys_read(0, buf, 1) > 0) {
        sys_write(1, buf, 1);
        for (int i = 0; i < nfds; i++) sys_write(fds[i], buf, 1);
    }
    for (int i = 0; i < nfds; i++) sys_close(fds[i]);
    return 0;
}

static int basename_cmd(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *p = rfind_char(argv[1], '/');
    write_str(1, p ? p + 1 : argv[1]);
    write_str(1, "\n");
    return 0;
}

static int dirname_cmd(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *p = rfind_char(argv[1], '/');
    if (!p) { write_str(1, ".\n"); return 0; }
    if (p == argv[1]) { write_str(1, "/\n"); return 0; }
    write_buf(1, argv[1], p - argv[1]);
    write_str(1, "\n");
    return 0;
}

static int dd_cmd(int argc, char **argv) {
    const char *ifile = 0, *ofile = 0;
    int bs = 512, count = 0, skip = 0, seek = 0;
    for (int i = 1; i < argc; i++) {
        if (str_ncmp(argv[i], "if=", 3) == 0) ifile = argv[i] + 3;
        else if (str_ncmp(argv[i], "of=", 3) == 0) ofile = argv[i] + 3;
        else if (str_ncmp(argv[i], "bs=", 3) == 0) bs = parse_int((const char **)&argv[i] + 3);
        else if (str_ncmp(argv[i], "count=", 6) == 0) count = parse_int((const char **)&argv[i] + 6);
        else if (str_ncmp(argv[i], "skip=", 5) == 0) skip = parse_int((const char **)&argv[i] + 5);
        else if (str_ncmp(argv[i], "seek=", 5) == 0) seek = parse_int((const char **)&argv[i] + 5);
    }
    int in_fd = ifile ? sys_open(ifile, LINUX_O_RDONLY, 0) : 0;
    int out_fd = ofile ? sys_open(ofile, LINUX_O_WRONLY | LINUX_O_CREAT, 0644) : 1;
    if (in_fd < 0 || out_fd < 0) { write_str(2, "dd: cannot open\n"); return 1; }
    if (skip) sys_lseek(in_fd, skip * bs, LINUX_SEEK_SET);
    if (seek && out_fd != 1) sys_lseek(out_fd, seek * bs, LINUX_SEEK_SET);
    char buf[BUF_SIZE];
    int total = 0, blocks = 0;
    while (!count || blocks < count) {
        int n = sys_read(in_fd, buf, bs > BUF_SIZE ? BUF_SIZE : bs);
        if (n <= 0) break;
        sys_write(out_fd, buf, n);
        total += n; blocks++;
    }
    if (ifile) sys_close(in_fd);
    if (ofile) sys_close(out_fd);
    {
        char num[32];
        int pos = 0, n = blocks;
        do { num[pos++] = '0' + n % 10; n /= 10; } while (n);
        while (pos > 0) write_buf(1, &num[--pos], 1);
        write_str(1, "+0 records in\n");
        pos = 0; n = blocks;
        do { num[pos++] = '0' + n % 10; n /= 10; } while (n);
        while (pos > 0) write_buf(1, &num[--pos], 1);
        write_str(1, "+0 records out\n");
    }
    return 0;
}

static int cmp_cmd(int argc, char **argv) {
    if (argc < 3) { write_str(2, "cmp: missing operand\n"); return 1; }
    int fd1 = sys_open(argv[1], LINUX_O_RDONLY, 0);
    int fd2 = sys_open(argv[2], LINUX_O_RDONLY, 0);
    if (fd1 < 0 || fd2 < 0) { write_str(2, "cmp: cannot open\n"); return 1; }
    char b1[BUF_SIZE], b2[BUF_SIZE];
    int off = 0;
    for (;;) {
        int n1 = read_all(fd1, b1, BUF_SIZE);
        int n2 = read_all(fd2, b2, BUF_SIZE);
        int n = n1 < n2 ? n1 : n2;
        for (int i = 0; i < n; i++) {
            if (b1[i] != b2[i]) {
                char num[32];
                int pos = 0, val = off + i;
                do { num[pos++] = '0' + val % 10; val /= 10; } while (val);
                write_str(1, argv[1]); write_str(1, " ");
                while (pos > 0) write_buf(1, &num[--pos], 1);
                write_str(1, "\n");
                sys_close(fd1); sys_close(fd2);
                return 1;
            }
        }
        if (n1 != n2) {
            write_str(1, "EOF on "); write_str(1, n1 < n2 ? argv[2] : argv[1]);
            write_str(1, "\n");
            sys_close(fd1); sys_close(fd2);
            return 1;
        }
        if (n1 == 0) break;
        off += n;
    }
    sys_close(fd1); sys_close(fd2);
    return 0;
}

static int seq_cmd(int argc, char **argv) {
    int start = 1, end = 1, step = 1;
    if (argc == 2) { end = parse_int((const char **)&argv[1]); }
    else if (argc == 3) { start = parse_int((const char **)&argv[1]); end = parse_int((const char **)&argv[2]); }
    else if (argc >= 4) { start = parse_int((const char **)&argv[1]); step = parse_int((const char **)&argv[2]); end = parse_int((const char **)&argv[3]); }
    char num[16];
    for (int i = start; (step > 0) ? (i <= end) : (i >= end); i += step) {
        int pos = 0, n = i;
        if (n < 0) { write_str(1, "-"); n = -n; }
        do { num[pos++] = '0' + n % 10; n /= 10; } while (n);
        while (pos > 0) write_buf(1, &num[--pos], 1);
        write_str(1, "\n");
    }
    return 0;
}

static int true_cmd(void) { return 0; }
static int false_cmd(void) { return 1; }

static int yes_cmd(int argc, char **argv) {
    const char *s = (argc > 1) ? argv[1] : "y";
    int slen = str_len(s);
    for (;;) { write_buf(1, s, slen); write_str(1, "\n"); }
    return 0;
}

static int whoami_cmd(void) {
    int uid = 0;
    uid = sys_geteuid32();
    if (uid == 0) write_str(1, "root\n");
    else {
        char num[16];
        int pos = 0;
        do { num[pos++] = '0' + uid % 10; uid /= 10; } while (uid);
        while (pos > 0) write_buf(1, &num[--pos], 1);
        write_str(1, "\n");
    }
    return 0;
}

static int hostname_cmd(void) {
    struct linux_utsname uts;
    if (sys_uname(&uts) < 0) return 1;
    write_str(1, uts.nodename);
    write_str(1, "\n");
    return 0;
}

static int test_cmd(int argc, char **argv) {
    if (argc < 2) return 1;
    if (str_cmp(argv[1], "-f") == 0 && argc > 2) {
        struct linux_stat64 st;
        return sys_stat64(argv[2], &st) < 0 ? 1 : 0;
    }
    if (str_cmp(argv[1], "-d") == 0 && argc > 2) {
        struct linux_stat64 st;
        if (sys_stat64(argv[2], &st) < 0) return 1;
        return (st.st_mode & LINUX_S_IFDIR) ? 0 : 1;
    }
    if (str_cmp(argv[1], "-e") == 0 && argc > 2) {
        struct linux_stat64 st;
        return sys_stat64(argv[2], &st) < 0 ? 1 : 0;
    }
    if (str_cmp(argv[1], "-n") == 0 && argc > 2) return argv[2][0] ? 0 : 1;
    if (str_cmp(argv[1], "-z") == 0 && argc > 2) return argv[2][0] ? 1 : 0;
    if (argc == 2) return argv[1][0] ? 0 : 1;
    if (argc == 4) {
        int a = parse_int((const char **)&argv[1]);
        int b = parse_int((const char **)&argv[3]);
        if (str_cmp(argv[2], "-eq") == 0) return a == b ? 0 : 1;
        if (str_cmp(argv[2], "-ne") == 0) return a != b ? 0 : 1;
        if (str_cmp(argv[2], "-gt") == 0) return a > b ? 0 : 1;
        if (str_cmp(argv[2], "-lt") == 0) return a < b ? 0 : 1;
    }
    return 1;
}

static int readlink_cmd(int argc, char **argv) {
    if (argc < 2) { write_str(2, "readlink: missing operand\n"); return 1; }
    char buf[256];
    int n = sys_readlink(argv[1], buf, sizeof(buf) - 1);
    if (n < 0) { write_str(2, "readlink: cannot read link\n"); return 1; }
    buf[n] = '\0';
    write_str(1, buf);
    write_str(1, "\n");
    return 0;
}

static int link_cmd(int argc, char **argv) {
    if (argc < 3) { write_str(2, "link: missing operand\n"); return 1; }
    return sys_link(argv[1], argv[2]) < 0 ? 1 : 0;
}

typedef struct { const char *name; int (*func)(int, char **); } cmd_entry_t;
typedef struct { const char *name; int (*func0)(void); } cmd0_entry_t;

static const cmd_entry_t cmds[] = {
    {"cp", cp_cmd}, {"mv", mv_cmd}, {"rm", rm_cmd}, {"ln", ln_cmd},
    {"chmod", chmod_cmd}, {"touch", touch_cmd},
    {"head", head_cmd}, {"tail", tail_cmd}, {"wc", wc_cmd},
    {"sort", sort_cmd}, {"uniq", uniq_cmd}, {"cut", cut_cmd},
    {"tr", tr_cmd}, {"tee", tee_cmd},
    {"basename", basename_cmd}, {"dirname", dirname_cmd},
    {"dd", dd_cmd}, {"cmp", cmp_cmd}, {"seq", seq_cmd},
    {"test", test_cmd}, {"readlink", readlink_cmd}, {"link", link_cmd},
    {"yes", yes_cmd},
};

static const cmd0_entry_t cmds0[] = {
    {"true", true_cmd}, {"false", false_cmd},
    {"whoami", whoami_cmd}, {"hostname", hostname_cmd},
};

static const char *base_name(const char *p) {
    const char *s = rfind_char(p, '/');
    return s ? s + 1 : p;
}

int main(int argc, char **argv) {
    const char *cmd = base_name(argv[0]);
    if (argc > 1 && str_cmp(argv[1], "--help") == 0) {
        write_str(1, "OpenHobbyOS Core Utils\n");
        return 0;
    }
    for (unsigned int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        if (str_cmp(cmd, cmds[i].name) == 0) return cmds[i].func(argc, argv);
    }
    for (unsigned int i = 0; i < sizeof(cmds0) / sizeof(cmds0[0]); i++) {
        if (str_cmp(cmd, cmds0[i].name) == 0) return cmds0[i].func0();
    }
    write_str(2, "coreutils: unknown command: ");
    write_str(2, cmd);
    write_str(2, "\n");
    return 1;
}
