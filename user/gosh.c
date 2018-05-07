#include "runtime.h"
#include "syscall.h"

#define LINE_MAX    1024
#define ARG_MAX     64
#define PATH_MAX    4096
#define HIST_MAX    16
#define THEME_FILE  "/etc/shell-colors.jsonc"
#define MAX(x,y)    ((x)>(y)?(x):(y))
#define GOSH_ERR_PERM  (-1)
#define GOSH_ERR_ACCES (-13)

struct gosh_theme {
    int user, host, path, symbol, error, success, default_c, warning, title;
};

static struct gosh_theme g_thm;
static int g_last_exit;
static char g_cwd[PATH_MAX];
static char **g_envp;
static char g_history[HIST_MAX][LINE_MAX];
static int g_hist_count;
static int g_hist_pos;
static char g_session_user[32];
static char g_session_home[PATH_MAX];
static char g_session_shell[PATH_MAX];
static int g_logged_in;
static int g_request_relogin;

static int read_byte(void) {
    unsigned char ch = 0;
    int n = sys_read(0, (char*)&ch, 1);
    if (n == 1) return (int)ch;
    return (n == 0) ? -2 : -1;
}

static void write_ch(char ch) { sys_write(1, &ch, 1); }

static void write_str(const char *s) { u_write_buffer(s, u_strlen(s)); }

static int parse_json_int(const char *buf, const char *key, int fallback) {
    const char *p = buf;
    while (*p) {
        const char *k = key;
        const char *scan = p;
        while (*k && *scan && *scan == *k) { k++; scan++; }
        if (!*k) {
            while (*scan && *scan != ':') scan++;
            if (*scan != ':') { p++; continue; }
            scan++;
            while (*scan && (*scan == ' ' || *scan == '\t')) scan++;
            if (*scan == '"') scan++;
            int val = 0;
            while (*scan && *scan >= '0' && *scan <= '9')
                val = val * 10 + (*scan++ - '0');
            return val;
        }
        p++;
    }
    return fallback;
}

static void load_theme(void) {
    g_thm.user = 96; g_thm.host = 92; g_thm.path = 94;
    g_thm.symbol = 97; g_thm.error = 91; g_thm.success = 92;
    g_thm.default_c = 97; g_thm.warning = 93; g_thm.title = 96;

    int fd = sys_open(THEME_FILE, 0, 0);
    if (fd < 0) return;
    char tbuf[2048];
    int total = 0;
    for (;;) {
        int n = sys_read(fd, tbuf + total, 1);
        if (n <= 0) break;
        total++;
        if (total >= (int)sizeof(tbuf) - 1) break;
    }
    sys_close(fd);
    tbuf[total] = '\0';
    g_thm.user = parse_json_int(tbuf, "prompt_user", g_thm.user);
    g_thm.host = parse_json_int(tbuf, "prompt_host", g_thm.host);
    g_thm.path = parse_json_int(tbuf, "prompt_path", g_thm.path);
    g_thm.symbol = parse_json_int(tbuf, "prompt_symbol", g_thm.symbol);
    g_thm.error = parse_json_int(tbuf, "error", g_thm.error);
    g_thm.success = parse_json_int(tbuf, "success", g_thm.success);
    g_thm.default_c = parse_json_int(tbuf, "default", g_thm.default_c);
    g_thm.warning = parse_json_int(tbuf, "warning", g_thm.warning);
    g_thm.title = parse_json_int(tbuf, "title", g_thm.title);
}

static const char *env_get(const char *name) {
    if (!g_envp) return 0;
    unsigned int nlen = u_strlen(name);
    for (int i = 0; g_envp[i]; i++)
        if (u_strncmp(g_envp[i], name, nlen) == 0 && g_envp[i][nlen] == '=')
            return g_envp[i] + nlen + 1;
    return 0;
}

static void write_buf(const char *buf, unsigned int len) { sys_write(1, buf, len); }

static unsigned int emit_sgr(char *buf, int code) {
    unsigned int pos = 0;
    buf[pos++] = 27; buf[pos++] = '[';
    if (code >= 90 && code <= 97) {
        buf[pos++] = '1'; buf[pos++] = ';';
        code -= 60;
    }
    if (code == 0) {
        buf[pos++] = '0';
    } else {
        char tmp[8];
        unsigned int tpos = 0;
        int c = code;
        while (c) { tmp[tpos++] = (char)('0' + (c % 10)); c /= 10; }
        while (tpos) buf[pos++] = tmp[--tpos];
    }
    buf[pos++] = 'm';
    return pos;
}

static unsigned int emit_str(char *buf, const char *s) {
    unsigned int pos = 0;
    while (*s) buf[pos++] = *s++;
    return pos;
}

static void show_prompt(void) {
    const char *user = g_logged_in && g_session_user[0] ? g_session_user : "root";
    const char *host = env_get("HOSTNAME");
    if (!host) host = "openhobby";
    int is_root = (u_strcmp(user, "root") == 0);

    if (!sys_getcwd(g_cwd, sizeof(g_cwd)))
        u_strcpy(g_cwd, "?");

    const char *cwd_short = g_cwd;
    const char *slash = 0;
    const char *p = g_cwd;
    while (*p) { if (*p == '/') slash = p; p++; }
    if (slash && slash != g_cwd) cwd_short = slash + 1;

    char prom[512];
    unsigned int pos = 0;

    if (g_last_exit != 0) {
        pos += emit_sgr(prom + pos, g_thm.error);
        char tmp[16]; unsigned int tp = 0; int ec = g_last_exit;
        if (ec == 0) { tmp[tp++] = '0'; }
        else { char rev[8]; unsigned int rp = 0;
            while (ec) { rev[rp++] = (char)('0' + (ec % 10)); ec /= 10; }
            while (rp) tmp[tp++] = rev[--rp]; }
        tmp[tp] = ' '; tmp[tp + 1] = '\0';
        pos += emit_str(prom + pos, tmp);
    }

    pos += emit_sgr(prom + pos, g_thm.user);
    pos += emit_str(prom + pos, user);
    pos += emit_sgr(prom + pos, g_thm.symbol);
    prom[pos++] = '@';
    pos += emit_sgr(prom + pos, g_thm.host);
    pos += emit_str(prom + pos, host);
    pos += emit_sgr(prom + pos, g_thm.symbol);
    prom[pos++] = ':';
    pos += emit_sgr(prom + pos, g_thm.path);
    pos += emit_str(prom + pos, cwd_short);
    pos += emit_sgr(prom + pos, g_thm.symbol);
    prom[pos++] = ' '; prom[pos++] = (char)(is_root ? '#' : '$'); prom[pos++] = ' ';
    pos += emit_sgr(prom + pos, 0);

    write_buf(prom, pos);
}

static void hist_add(const char *line) {
    if (!*line) return;
    if (g_hist_count > 0 && u_strcmp(g_history[(g_hist_count-1) % HIST_MAX], line) == 0)
        return;
    u_strcpy(g_history[g_hist_count % HIST_MAX], line);
    g_hist_count++;
    g_hist_pos = g_hist_count;
}

static void write_decimal(char *buf, unsigned int *pos, unsigned int value) {
    char tmp[16];
    unsigned int used = 0;

    if (value == 0) {
        buf[(*pos)++] = '0';
        return;
    }

    while (value && used < sizeof(tmp)) {
        tmp[used++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (used) {
        buf[(*pos)++] = tmp[--used];
    }
}

static void write_cursor_left(unsigned int count) {
    char seq[16];
    unsigned int pos = 0;

    if (count == 0) {
        return;
    }

    seq[pos++] = '\e';
    seq[pos++] = '[';
    write_decimal(seq, &pos, count);
    seq[pos++] = 'D';
    write_buf(seq, pos);
}

static void redraw_shell_line(const char *buf, int len, int cursor) {
    write_str("\r\e[K");
    show_prompt();
    if (len > 0) {
        write_buf(buf, (unsigned int)len);
    }
    if (cursor < len) {
        write_cursor_left((unsigned int)(len - cursor));
    }
}

struct prompt_redraw_ctx {
    const char *prompt;
    int echo;
};

static void redraw_prompt_line(const char *prompt, const char *buf, int len, int cursor, int echo) {
    write_str("\r\e[K");
    write_str(prompt);
    if (len > 0) {
        if (echo) {
            write_buf(buf, (unsigned int)len);
        } else {
            for (int i = 0; i < len; i++) {
                write_ch('*');
            }
        }
    }
    if (cursor < len) {
        write_cursor_left((unsigned int)(len - cursor));
    }
}

static void shell_set_session(const char *username) {
    if (!username || !*username) {
        username = "root";
    }

    u_strcpy(g_session_user, username);
    if (u_strcmp(username, "root") == 0) {
        u_strcpy(g_session_home, "/root");
        u_strcpy(g_session_shell, "/bin/gosh");
    } else {
        u_strcpy(g_session_home, "/home/user");
        u_strcpy(g_session_shell, "/bin/gosh");
    }
    g_logged_in = 1;
}

static int read_line_common(char *buf, int max, int echo, int use_history, void (*redraw)(const char *, int, int, void *), void *redraw_ctx) {
    int pos = 0;
    int cursor = 0;
    int history_index = g_hist_count;
    int editing_history = 0;

    if (!buf || max < 2) {
        return -1;
    }

    buf[0] = '\0';
    redraw(buf, pos, cursor, redraw_ctx);
    for (;;) {
        int ch = read_byte();
        if (ch == -2) {
            sys_sched_yield();
            continue;
        }
        if (ch < 0) {
            return -1;
        }
        if (ch == '\n' || ch == '\r') {
            write_ch('\n');
            buf[pos] = '\0';
            if (use_history) {
                hist_add(buf);
            }
            return pos;
        }
        if (ch == 127 || ch == 8) {
            if (cursor > 0) {
                for (int i = cursor - 1; i < pos - 1; ++i) {
                    buf[i] = buf[i + 1];
                }
                cursor--;
                pos--;
                buf[pos] = '\0';
                if (use_history && editing_history) {
                    editing_history = 0;
                }
                redraw(buf, pos, cursor, redraw_ctx);
            }
            continue;
        }
        if (ch == 3) {
            buf[0] = '\0';
            pos = 0;
            cursor = 0;
            editing_history = 0;
            redraw(buf, pos, cursor, redraw_ctx);
            continue;
        }
        if (ch == 21) {
            pos = 0;
            cursor = 0;
            buf[0] = '\0';
            editing_history = 0;
            redraw(buf, pos, cursor, redraw_ctx);
            continue;
        }
        if (ch == 11) {
            pos = cursor;
            buf[pos] = '\0';
            editing_history = 0;
            redraw(buf, pos, cursor, redraw_ctx);
            continue;
        }
        if (ch == 1) {
            cursor = 0;
            redraw(buf, pos, cursor, redraw_ctx);
            continue;
        }
        if (ch == 5) {
            cursor = pos;
            redraw(buf, pos, cursor, redraw_ctx);
            continue;
        }
        if (ch == 27) {
            int n1 = read_byte();
            if (n1 == -2) {
                continue;
            }
            if (n1 == '[' || n1 == 'O') {
                int n2 = read_byte();
                if (n2 == -2) {
                    continue;
                }
                if (n2 == 'A' && use_history) {
                    if (g_hist_count > 0 && history_index > 0) {
                        history_index--;
                        u_strcpy(buf, g_history[history_index % HIST_MAX]);
                        pos = (int)u_strlen(buf);
                        cursor = pos;
                        editing_history = 1;
                        redraw(buf, pos, cursor, redraw_ctx);
                    }
                    continue;
                }
                if (n2 == 'B' && use_history) {
                    if (g_hist_count > 0 && history_index < g_hist_count - 1) {
                        history_index++;
                        u_strcpy(buf, g_history[history_index % HIST_MAX]);
                        pos = (int)u_strlen(buf);
                        cursor = pos;
                        editing_history = 1;
                        redraw(buf, pos, cursor, redraw_ctx);
                    } else if (history_index == g_hist_count - 1) {
                        history_index = g_hist_count;
                        pos = 0;
                        cursor = 0;
                        buf[0] = '\0';
                        editing_history = 0;
                        redraw(buf, pos, cursor, redraw_ctx);
                    }
                    continue;
                }
                if (n2 == 'C') {
                    if (cursor < pos) {
                        cursor++;
                        redraw(buf, pos, cursor, redraw_ctx);
                    }
                    continue;
                }
                if (n2 == 'D') {
                    if (cursor > 0) {
                        cursor--;
                        redraw(buf, pos, cursor, redraw_ctx);
                    }
                    continue;
                }
                if (n2 == 'H') {
                    cursor = 0;
                    redraw(buf, pos, cursor, redraw_ctx);
                    continue;
                }
                if (n2 == 'F') {
                    cursor = pos;
                    redraw(buf, pos, cursor, redraw_ctx);
                    continue;
                }
                if (n2 == '3') {
                    int n3 = read_byte();
                    if (n3 == '~' && cursor < pos) {
                        for (int i = cursor; i < pos - 1; ++i) {
                            buf[i] = buf[i + 1];
                        }
                        pos--;
                        buf[pos] = '\0';
                        editing_history = 0;
                        redraw(buf, pos, cursor, redraw_ctx);
                    }
                    continue;
                }
            }
            continue;
        }
        if (ch < 32) {
            continue;
        }
        if (pos >= max - 1) {
            continue;
        }

        if (cursor == pos) {
            buf[pos++] = (char)ch;
            cursor = pos;
            buf[pos] = '\0';
            if (echo) {
                write_ch((char)ch);
            }
        } else {
            for (int i = pos; i > cursor; --i) {
                buf[i] = buf[i - 1];
            }
            buf[cursor] = (char)ch;
            pos++;
            cursor++;
            buf[pos] = '\0';
            editing_history = 0;
            redraw(buf, pos, cursor, redraw_ctx);
            continue;
        }
    }
}

static void redraw_shell_adapter(const char *buf, int len, int cursor, void *ctx) {
    (void)ctx;
    redraw_shell_line(buf, len, cursor);
}

static void redraw_prompt_adapter(const char *buf, int len, int cursor, void *ctx) {
    const struct prompt_redraw_ctx *prompt = (const struct prompt_redraw_ctx *)ctx;
    redraw_prompt_line(prompt->prompt, buf, len, cursor, prompt->echo);
}

static int read_line(char *buf, int max) {
    return read_line_common(buf, max, 1, 1, redraw_shell_adapter, NULL);
}

static int read_prompt_line(const char *prompt, char *buf, int max, int echo) {
    struct prompt_redraw_ctx ctx = { prompt, echo };
    return read_line_common(buf, max, echo, 0, redraw_prompt_adapter, &ctx);
}

static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
}

static int parse_line(char *line, const char **argv) {
    int argc = 0;
    char *wp = line;
    char *rp = line;
    char quote = 0;

    while (*rp) {
        skip_ws((const char **)&rp);
        if (!*rp) break;
        if (argc >= ARG_MAX - 1) return -1;

        argv[argc++] = wp;
        quote = 0;

        while (*rp) {
            char ch = *rp++;
            if (quote) {
                if (ch == quote) { quote = 0; continue; }
                if (ch == '\\' && quote == '"' && (*rp == '"' || *rp == '\\'))
                    ch = *rp++;
                *wp++ = ch;
                continue;
            }
            if (ch == '"' || ch == '\'') { quote = ch; continue; }
            if (ch == ' ' || ch == '\t') break;
            if (ch == '\\' && *rp) ch = *rp++;
            *wp++ = ch;
        }
        if (quote) return -1;
        *wp++ = '\0';
    }

    argv[argc] = 0;
    return argc;
}

static int find_in_path(const char *cmd, char *resolved, unsigned int size) {
    if (!cmd || !*cmd) return -1;
    if (cmd[0] == '/') {
        unsigned int i = 0;
        while (cmd[i] && i + 1 < size) { resolved[i] = cmd[i]; i++; }
        resolved[i] = '\0';
        {
            struct linux_stat64 st;
            if (sys_stat64(resolved, &st) != 0) {
                return -1;
            }
        }
        return (sys_access(resolved, LINUX_X_OK) == 0) ? 0 : GOSH_ERR_ACCES;
    }

    const char *path = env_get("PATH");
    if (!path) path = "/bin:/usr/bin";

    char path_copy[PATH_MAX];
    unsigned int pi = 0;
    while (*path && pi + 1 < sizeof(path_copy)) path_copy[pi++] = *path++;
    path_copy[pi] = '\0';

    char *start = path_copy;
    while (*start) {
        char *end = start;
        while (*end && *end != ':') end++;
        char sep = *end;
        *end = '\0';

        unsigned int si = 0;
        while (start[si] && si + 1 < size) { resolved[si] = start[si]; si++; }
        if (si > 0 && resolved[si - 1] != '/') {
            if (si + 1 < size) resolved[si++] = '/';
        }
        unsigned int ci = 0;
        while (cmd[ci] && si + 1 < size) { resolved[si++] = cmd[ci++]; }
        resolved[si] = '\0';

        struct linux_stat64 st;
        if (sys_stat64(resolved, &st) == 0) {
            if (sys_access(resolved, LINUX_X_OK) == 0) {
                return 0;
            }
            return GOSH_ERR_ACCES;
        }

        if (!sep) break;
        start = end + 1;
    }
    return -1;
}

static int builtin_cd(int argc, const char **argv) {
    (void)argc;
    const char *target = argv[1] ? argv[1] : (g_session_home[0] ? g_session_home : "/");
    if (sys_chdir(target) < 0) {
        write_str("gosh: cd: "); write_str(target);
        write_str(": No such file or directory\n");
        return 1;
    }
    return 0;
}

static int builtin_exit(int argc, const char **argv) {
    int code = 0;
    if (argc > 1) { int ok = 0; code = (int)u_parse_uint(argv[1], &ok);
        if (!ok) code = 0; }
    g_last_exit = -1;
    return code;
}

static int builtin_help(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str("GOSH! - OpenHobbyOS Shell\n");
    write_str("Built-in commands:\n");
    write_str("  cd <dir>     Change directory\n");
    write_str("  clear        Clear the screen\n");
    write_str("  echo <text>  Print text\n");
    write_str("  exit [code]  Exit the shell\n");
    write_str("  help         Display this help\n");
    write_str("  history      Show command history\n");
    write_str("  id           Show current identity\n");
    write_str("  logout       Return to the login screen\n");
    write_str("  chmod MODE PATH...  Change file mode\n");
    write_str("  sudo CMD...  Run a command as root\n");
    write_str("  pwd          Print working directory\n");
    write_str("  export       Print environment variables\n");
    return 0;
}

static int builtin_clear(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str("\e[H\e[J");
    return 0;
}

static int builtin_pwd(int argc, const char **argv) {
    (void)argc; (void)argv;
    if (sys_getcwd(g_cwd, sizeof(g_cwd))) {
        write_str(g_cwd); write_ch('\n');
    }
    return 0;
}

static int builtin_echo(int argc, const char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) write_ch(' ');
        write_str(argv[i]);
    }
    write_ch('\n');
    return 0;
}

static int builtin_export(int argc, const char **argv) {
    (void)argv;
    if (argc == 1) {
        for (int i = 0; g_envp && g_envp[i]; i++) {
            write_str(g_envp[i]); write_ch('\n');
        }
        return 0;
    }
    write_str("gosh: export: setting variables not supported\n");
    return 1;
}

static int builtin_id(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str("uid="); u_put_uint((unsigned int)sys_getuid32());
    write_str(" gid="); u_put_uint((unsigned int)sys_getgid32());
    write_str(" euid="); u_put_uint((unsigned int)sys_geteuid32());
    write_str(" egid="); u_put_uint((unsigned int)sys_getegid32());
    write_str("\n");
    return 0;
}

static int builtin_whoami(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str(g_session_user[0] ? g_session_user : "root");
    write_str("\n");
    return 0;
}

static int builtin_history(int argc, const char **argv) {
    (void)argc; (void)argv;
    int start = MAX(0, g_hist_count - HIST_MAX);
    for (int i = start; i < g_hist_count; i++) {
        char num[12]; unsigned int np = 0; int n = i + 1;
        char rev[8]; unsigned int rp = 0;
        while (n) { rev[rp++] = (char)('0' + (n % 10)); n /= 10; }
        while (rp) num[np++] = rev[--rp];
        num[np] = '\0';
        write_str(num); write_str("  "); write_str(g_history[i % HIST_MAX]); write_ch('\n');
    }
    return 0;
}

static unsigned int parse_octal_mode(const char *text, int *ok) {
    unsigned int mode = 0;

    if (ok) {
        *ok = 0;
    }
    if (!text || !*text) {
        return 0;
    }
    while (*text) {
        if (*text < '0' || *text > '7') {
            return 0;
        }
        mode = (mode << 3) | (unsigned int)(*text - '0');
        text++;
    }
    if (ok) {
        *ok = 1;
    }
    return mode;
}

static int execute_command_argv(int argc, const char **argv);

static int builtin_logout(int argc, const char **argv) {
    (void)argc; (void)argv;
    g_request_relogin = 1;
    return 0;
}

static int builtin_chmod(int argc, const char **argv) {
    int ok = 0;
    unsigned int mode;
    int failed = 0;

    if (argc < 3) {
        write_str("chmod: usage: chmod MODE FILE...\n");
        return 1;
    }

    mode = parse_octal_mode(argv[1], &ok);
    if (!ok) {
        write_str("chmod: invalid mode\n");
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        if (sys_chmod(argv[i], (int)mode) < 0) {
            write_str("chmod: "); write_str(argv[i]); write_str(": failed\n");
            failed = 1;
        }
    }

    return failed;
}

static int builtin_sudo(int argc, const char **argv) {
    char password[64];
    unsigned int old_uid;
    unsigned int old_gid;
    unsigned int old_euid;
    unsigned int old_egid;
    int rc;

    if (argc < 2) {
        write_str("sudo: usage: sudo COMMAND [ARGS...]\n");
        return 1;
    }

    if (sys_geteuid32() == 0) {
        return execute_command_argv(argc - 1, argv + 1);
    }

    if (read_prompt_line("sudo password for root: ", password, sizeof(password), 0) < 0) {
        write_str("\nsudo: failed to read password\n");
        return 1;
    }

    old_uid = (unsigned int)sys_getuid32();
    old_gid = (unsigned int)sys_getgid32();
    old_euid = (unsigned int)sys_geteuid32();
    old_egid = (unsigned int)sys_getegid32();

    if (sys_auth("root", password) < 0) {
        write_str("sudo: authentication failed\n");
        return 1;
    }

    rc = execute_command_argv(argc - 1, argv + 1);

    sys_setuid(old_uid);
    sys_setgid(old_gid);
    sys_seteuid(old_euid);
    sys_setegid(old_egid);

    return rc;
}

struct builtin {
    const char *name;
    int (*func)(int argc, const char **argv);
};

static const struct builtin builtins[] = {
    {"cd",      builtin_cd},
    {"clear",   builtin_clear},
    {"chmod",   builtin_chmod},
    {"echo",    builtin_echo},
    {"exit",    builtin_exit},
    {"export",  builtin_export},
    {"help",    builtin_help},
    {"history", builtin_history},
    {"pwd",     builtin_pwd},
    {"id",      builtin_id},
    {"logout",  builtin_logout},
    {"sudo",    builtin_sudo},
    {"whoami",  builtin_whoami},
    {0, 0}
};

static int run_builtin(const char *name, int argc, const char **argv) {
    for (const struct builtin *b = builtins; b->name; b++)
        if (u_strcmp(name, b->name) == 0) return b->func(argc, argv);
    return -1;
}

static int execute_external(const char *path, const char **argv) {
    int pid = sys_spawn(path, argv);
    if (pid < 0) {
        if (pid == GOSH_ERR_ACCES || pid == GOSH_ERR_PERM) {
            write_str("gosh: "); write_str(path); write_str(": permission denied\n");
            return 126;
        }
        write_str("gosh: "); write_str(path); write_str(": command not found\n");
        return 127;
    }
    int status = 0;
    if (sys_waitpid(pid, &status, 0) < 0) return 127;
    return status;
}

static int execute_command_argv(int argc, const char **argv) {
    if (argc <= 0 || !argv || !argv[0] || !*argv[0]) {
        return 0;
    }

    char resolved[PATH_MAX];
    int path_result = find_in_path(argv[0], resolved, sizeof(resolved));
    if (path_result == 0) {
        return execute_external(resolved, argv);
    }
    if (path_result == GOSH_ERR_ACCES || path_result == GOSH_ERR_PERM) {
        write_str("gosh: "); write_str(argv[0]); write_str(": permission denied\n");
        return 126;
    }

    int r = run_builtin(argv[0], argc, argv);
    if (r >= 0) {
        return r;
    }

    write_str("gosh: "); write_str(argv[0]); write_str(": command not found\n");
    return 127;
}

static void execute_line(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == '#') return;

    const char *argv[ARG_MAX];
    int argc = parse_line(line, argv);
    if (argc <= 0) return;

    g_last_exit = execute_command_argv(argc, argv);
}

static int shell_login(void) {
    char username[32];
    char password[64];

    g_logged_in = 0;
    g_session_user[0] = '\0';
    g_session_home[0] = '\0';
    g_session_shell[0] = '\0';

    for (;;) {
        write_str("\e[H\e[J");
        write_str("OpenHobbyOS Display Manager\n");
        write_str("Available accounts: root, user\n\n");

        if (read_prompt_line("Username: ", username, sizeof(username), 1) < 0) {
            return -1;
        }
        if (username[0] == '\0') {
            continue;
        }
        if (read_prompt_line("Password: ", password, sizeof(password), 0) < 0) {
            return -1;
        }

        if (sys_auth(username, password) == 0) {
            shell_set_session(username);
            g_last_exit = 0;
            g_hist_count = 0;
            g_hist_pos = 0;
            g_request_relogin = 0;
            if (g_session_home[0] && sys_chdir(g_session_home) < 0) {
                sys_chdir("/");
            }
            write_str("\e[H\e[J");
            write_str("Login successful for ");
            write_str(g_session_user);
            write_str("\n");
            return 0;
        }

        write_str("Login incorrect\n");
    }
}

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv;
    g_envp = envp;
    g_last_exit = 0;
    g_hist_count = 0;
    g_hist_pos = 0;

    load_theme();
    u_strcpy(g_session_user, "root");
    u_strcpy(g_session_home, "/root");
    u_strcpy(g_session_shell, "/bin/gosh");
    g_logged_in = 0;

    write_str("GOSH! v2.0 - OpenHobbyOS Shell\n");

    if (shell_login() < 0) {
        return 1;
    }

    char line[LINE_MAX];

    for (;;) {
        show_prompt();
        int len = read_line(line, sizeof(line));
        if (len < 0) break;
        execute_line(line);
        if (g_last_exit < 0) break;
        if (g_request_relogin) {
            if (shell_login() < 0) {
                break;
            }
        }
    }

    return (g_last_exit == -1) ? 0 : g_last_exit;
}
