#include "runtime.h"
#include "syscall.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#ifndef LINE_MAX
#define LINE_MAX    1024
#endif
#ifndef ARG_MAX
#define ARG_MAX     64
#endif
#ifndef PATH_MAX
#define PATH_MAX    4096
#endif
#define HIST_MAX    16
#define THEME_FILE  "/etc/shell-colors.jsonc"
#define MAX(x,y)    ((x)>(y)?(x):(y))
#define GOSH_ERR_PERM  (-1)
#define GOSH_ERR_ACCES (-13)
#define GOSH_ERR_NOTFOUND (-127)
#define FONT_PATH   "/fonts/Monospace.ttf"
#define FONT_SIZE   16
#define TAB_WIDTH   4
#define TERM_ROWS   10000
#define TERM_COLS   256

static int g_last_exit;
static int g_exit_code;
static int g_use_colors = 1;
static int g_prompt_needs_spacing;
static char g_cwd[PATH_MAX];
static char **g_envp;
static char g_history[HIST_MAX][LINE_MAX];
static int g_hist_count;
static int g_hist_pos;
static char g_session_user[32];
static char g_session_home[PATH_MAX];
static char g_session_shell[PATH_MAX];
static int g_logged_in;

static cairo_surface_t *g_cs;
static cairo_t *g_cr;
static cairo_font_face_t *g_font;
static int g_font_w, g_font_h, g_font_ascent;
static int g_win_w, g_win_h;
static int g_vis_rows, g_vis_cols;

static uint32_t *g_fb_map;
static int g_fb_fd = -1;
#define FBUF_SIZE (512 * 1024)
static char g_font_data[FBUF_SIZE];
#define PIXEL_W 800
#define PIXEL_H 600
static uint32_t g_pixels[PIXEL_W * PIXEL_H];

static char g_term_buf[TERM_ROWS][TERM_COLS];
static unsigned char g_term_attr[TERM_ROWS][TERM_COLS];
static int g_term_fg;
static int g_term_bg;
static int g_term_row;
static int g_term_col;
static int g_term_count;

static char g_input_buf[LINE_MAX];
static int g_input_pos;
static int g_input_cursor;

static int g_dirty;

static const double g_colors[][3] = {
    {0.19, 0.20, 0.22}, {0.80, 0.20, 0.18}, {0.30, 0.72, 0.36},
    {0.80, 0.62, 0.16}, {0.25, 0.46, 0.76}, {0.58, 0.29, 0.64},
    {0.17, 0.60, 0.66}, {0.80, 0.80, 0.80},
    {0.40, 0.42, 0.44}, {1.0, 0.33, 0.27}, {0.44, 0.88, 0.48},
    {1.0, 0.80, 0.22}, {0.37, 0.62, 0.92}, {0.76, 0.40, 0.80},
    {0.27, 0.74, 0.80}, {1.0, 1.0, 1.0},
};

struct gosh_theme { int user, host, path, symbol, error, success, default_c, warning, title; };
static struct gosh_theme g_thm;

static void pixel_fill(int x, int y, int w, int h, uint32_t color) {
    for (int row = y; row < y + h && row < g_win_h; row++) {
        if (row < 0) continue;
        for (int col = x; col < x + w && col < g_win_w; col++) {
            if (col < 0) continue;
            g_pixels[row * g_win_w + col] = color;
        }
    }
}

static void pixel_clear(uint32_t color) {
    unsigned int n = (unsigned int)(g_win_w * g_win_h);
    for (unsigned int i = 0; i < n; i++) g_pixels[i] = color;
}

static void term_clear(void) {
    g_term_count = g_term_row = g_term_col = 0;
    g_term_fg = 15; g_term_bg = 0;
    g_dirty = 1;
}

static void term_newline(void) {
    g_term_row++; g_term_col = 0;
    if (g_term_row >= TERM_ROWS) {
        memmove(g_term_buf[0], g_term_buf[1], (TERM_ROWS - 1) * TERM_COLS);
        memmove(g_term_attr[0], g_term_attr[1], (TERM_ROWS - 1) * TERM_COLS);
        memset(g_term_buf[TERM_ROWS - 1], 0, TERM_COLS);
        memset(g_term_attr[TERM_ROWS - 1], 0, TERM_COLS);
        g_term_row = TERM_ROWS - 1;
    }
    if (g_term_row >= g_term_count) g_term_count = g_term_row + 1;
}

static void term_write(const char *data, int len) {
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '\n') { term_newline(); }
        else if (c == '\r') { g_term_col = 0; }
        else if (c == '\t') {
            int next = (g_term_col + TAB_WIDTH) & ~(TAB_WIDTH - 1);
            while (g_term_col < next && g_term_col < TERM_COLS - 1)
                g_term_buf[g_term_row][g_term_col++] = ' ';
        } else if (c == '\e') {
            i++; if (i >= len) break;
            if (data[i] == '[') {
                i++; int params[8] = {0}, pc = 0;
                while (i < len) {
                    if (data[i] >= '0' && data[i] <= '9') params[pc] = params[pc] * 10 + (data[i] - '0');
                    else if (data[i] == ';') { pc++; if (pc >= 8) break; }
                    else if (data[i] == 'm') {
                        for (int pi = 0; pi <= pc && pi < 8; pi++) {
                            int p = params[pi];
                            if (p == 0) { g_term_fg = 15; g_term_bg = 0; }
                            else if (p == 1) { if (g_term_fg < 8) g_term_fg += 8; }
                            else if (p == 22) { if (g_term_fg >= 8) g_term_fg -= 8; }
                            else if (p >= 30 && p <= 37) g_term_fg = p - 30;
                            else if (p >= 90 && p <= 97) g_term_fg = p - 90 + 8;
                            else if (p >= 40 && p <= 47) g_term_bg = p - 40;
                            else if (p >= 100 && p <= 107) g_term_bg = p - 100 + 8;
                        }
                        break;
                    }
                    else if (data[i] == 'J') { if (params[0] == 2) term_clear(); break; }
                    else if (data[i] == 'K') {
                        memset(g_term_buf[g_term_row] + g_term_col, 0, TERM_COLS - g_term_col);
                        memset(g_term_attr[g_term_row] + g_term_col, 0, TERM_COLS - g_term_col);
                        break;
                    }
                    else break;
                    i++;
                }
            }
        } else if (c >= 32) {
            if (g_term_col < TERM_COLS - 1) {
                g_term_buf[g_term_row][g_term_col] = (char)c;
                g_term_attr[g_term_row][g_term_col] = (unsigned char)((g_term_bg << 4) | g_term_fg);
                g_term_col++;
            }
        }
    }
    g_dirty = 1;
}

static int parse_json_int(const char *buf, const char *key, int fallback) {
    const char *p = buf;
    while (*p) {
        const char *k = key, *scan = p;
        while (*k && *scan && *scan == *k) { k++; scan++; }
        if (!*k) {
            while (*scan && *scan != ':') scan++;
            if (*scan != ':') { p++; continue; }
            scan++;
            while (*scan && (*scan == ' ' || *scan == '\t')) scan++;
            if (*scan == '"') scan++;
            int val = 0;
            while (*scan && *scan >= '0' && *scan <= '9') val = val * 10 + (*scan++ - '0');
            return val;
        }
        p++;
    }
    return fallback;
}

static void load_theme(void) {
    g_thm.user = 96; g_thm.host = 92; g_thm.path = 94; g_thm.symbol = 97;
    g_thm.error = 91; g_thm.success = 92; g_thm.default_c = 97; g_thm.warning = 93; g_thm.title = 96;
    int fd = sys_open(THEME_FILE, 0, 0); if (fd < 0) return;
    char tbuf[2048]; int total = 0;
    for (;;) { int n = sys_read(fd, tbuf + total, 1); if (n <= 0) break; total++; if (total >= (int)sizeof(tbuf)-1) break; }
    sys_close(fd); tbuf[total] = '\0';
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

static void detect_color_support(void) {
    const char *no_color = env_get("NO_COLOR");
    g_use_colors = 1;
    if (no_color && *no_color) { g_use_colors = 0; return; }
    const char *term = env_get("TERM");
    if (term && u_strcmp(term, "dumb") == 0) g_use_colors = 0;
}

static void write_ch(char ch) { term_write(&ch, 1); }
static void write_str(const char *s) { term_write(s, (int)u_strlen(s)); }

struct gosh_fb_bitfield { uint32_t offset, length, msb_right; };
struct gosh_fb_vinfo {
    uint32_t xres, yres, xres_virtual, yres_virtual, xoffset, yoffset, bits_per_pixel, grayscale;
    struct gosh_fb_bitfield red, green, blue, transp;
};

static void text_at(int x, int y, const char *text, double r, double g, double b) {
    cairo_set_source_rgb(g_cr, r, g, b);
    cairo_move_to(g_cr, (double)x, (double)(y + g_font_ascent));
    cairo_show_text(g_cr, text);
}

static int display_init(void) {
    g_fb_fd = sys_open("/dev/fb0", 2, 0);
    if (g_fb_fd < 0) return -1;

    struct gosh_fb_vinfo vinfo;
    memset(&vinfo, 0, sizeof(vinfo));
    if (sys_ioctl(g_fb_fd, 0x4600, &vinfo) < 0) {
        vinfo.xres = 800; vinfo.yres = 600; vinfo.bits_per_pixel = 24;
    }

    g_win_w = (int)vinfo.xres;
    g_win_h = (int)vinfo.yres;

    unsigned int fb_size = (unsigned int)(g_win_w * g_win_h * ((int)vinfo.bits_per_pixel / 8));
    g_fb_map = sys_mmap2(NULL, fb_size, 3, 0x01, g_fb_fd, 0);
    if (!g_fb_map || g_fb_map == (void *)-1) {
        sys_close(g_fb_fd); g_fb_fd = -1;
        return -1;
    }

    if (g_win_w > PIXEL_W || g_win_h > PIXEL_H) return -1;

    g_cs = cairo_image_surface_create_for_data(
        (unsigned char *)g_pixels, CAIRO_FORMAT_ARGB32,
        g_win_w, g_win_h, g_win_w * 4);
    if (!g_cs) return -1;

    g_cr = cairo_create(g_cs);
    if (!g_cr) return -1;

    FT_Library ft;
    if (FT_Init_FreeType(&ft) != 0) return -1;

    int fd = sys_open(FONT_PATH, 0, 0);
    if (fd < 0) return -1;
    int total = 0;
    for (;;) {
        int n = sys_read(fd, g_font_data + total, 1);
        if (n <= 0) break;
        total++;
        if (total >= FBUF_SIZE) break;
    }
    sys_close(fd);
    if (total <= 0) return -1;

    FT_Face face;
    if (FT_New_Memory_Face(ft, (const FT_Byte *)g_font_data, (FT_Long)total, 0, &face) != 0) return -1;
    if (FT_Set_Pixel_Sizes(face, 0, FONT_SIZE) != 0) return -1;

    g_font = cairo_ft_font_face_create_for_ft_face(face, 0);
    cairo_set_font_face(g_cr, g_font);
    cairo_set_font_size(g_cr, (double)FONT_SIZE);

    cairo_font_extents_t fe;
    cairo_font_extents(g_cr, &fe);
    g_font_h = (int)(fe.height + 0.5);
    g_font_ascent = (int)(fe.ascent + 0.5);

    cairo_text_extents_t te;
    cairo_text_extents(g_cr, "W", &te);
    g_font_w = (int)(te.x_advance + 0.5);
    if (g_font_w < 1) g_font_w = FONT_SIZE;

    g_vis_rows = g_win_h / g_font_h;
    g_vis_cols = g_win_w / g_font_w;

    term_clear();
    return 0;
}

static void fb_blit(void) {
    uint8_t *dst = (uint8_t *)g_fb_map;
    uint8_t *src = (uint8_t *)g_pixels;
    for (int y = 0; y < g_win_h; y++) {
        for (int x = 0; x < g_win_w; x++) {
            dst[x*3] = src[x*4];
            dst[x*3+1] = src[x*4+1];
            dst[x*3+2] = src[x*4+2];
        }
        dst += g_win_w * 3;
        src += g_win_w * 4;
    }
}

static void draw_prompt_line(int row, int *out_x) {
    int x = 0;
    int y = row * g_font_h;

    if (g_last_exit != 0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d ", g_last_exit);
        text_at(x, y, tmp, g_colors[1][0], g_colors[1][1], g_colors[1][2]);
        x += (int)u_strlen(tmp) * g_font_w;
    }

    const char *user = g_logged_in && g_session_user[0] ? g_session_user : "root";
    const char *host = env_get("HOSTNAME");
    if (!host) host = "openhobby";
    int is_root = (u_strcmp(user, "root") == 0);

    if (!sys_getcwd(g_cwd, sizeof(g_cwd))) u_strcpy(g_cwd, "?");
    const char *cwd_short = g_cwd;
    const char *slash = 0;
    const char *p = g_cwd;
    while (*p) { if (*p == '/') slash = p; p++; }
    if (slash && slash != g_cwd) cwd_short = slash + 1;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s@", user);
    text_at(x, y, buf, g_colors[9][0], g_colors[9][1], g_colors[9][2]);
    x += (int)u_strlen(buf) * g_font_w;

    snprintf(buf, sizeof(buf), "%s:", host);
    text_at(x, y, buf, g_colors[10][0], g_colors[10][1], g_colors[10][2]);
    x += (int)u_strlen(buf) * g_font_w;

    text_at(x, y, cwd_short, g_colors[12][0], g_colors[12][1], g_colors[12][2]);
    x += (int)u_strlen(cwd_short) * g_font_w;

    snprintf(buf, sizeof(buf), " %c ", is_root ? '#' : '$');
    text_at(x, y, buf, g_colors[15][0], g_colors[15][1], g_colors[15][2]);
    x += (int)u_strlen(buf) * g_font_w;

    int input_x = x;

    for (int i = 0; i < g_input_pos; i++) {
        char cb[2] = {g_input_buf[i], '\0'};
        text_at(x, y, cb, g_colors[15][0], g_colors[15][1], g_colors[15][2]);
        x += g_font_w;
    }

    int cursor_x = input_x + g_input_cursor * g_font_w;
    pixel_fill(cursor_x, row * g_font_h, g_font_w, g_font_h, 0xCCCCCC);
    {
        char ch = g_input_cursor < g_input_pos ? g_input_buf[g_input_cursor] : ' ';
        char cb[2] = {ch, '\0'};
        text_at(cursor_x, y, cb, g_colors[0][0], g_colors[0][1], g_colors[0][2]);
    }

    if (out_x) *out_x = input_x;
}

static void display_update(void) {
    pixel_clear(0x303136);

    int term_rows = g_vis_rows - 1;
    int start = g_term_count - term_rows;
    if (start < 0) start = 0;

    for (int i = start; i < g_term_count; i++) {
        int draw_row = i - start + 1;
        if (draw_row >= g_vis_rows) break;
        int x = 0;
        int y = draw_row * g_font_h;
        for (int ci = 0; ci < TERM_COLS; ci++) {
            unsigned char ch = (unsigned char)g_term_buf[i][ci];
            if (ch == 0) break;
            unsigned char attr = g_term_attr[i][ci];
            int fg = attr & 0x0F;
            if (fg > 15) fg = 15;
            char buf[2] = {(char)ch, '\0'};
            text_at(x, y, buf, g_colors[fg][0], g_colors[fg][1], g_colors[fg][2]);
            x += g_font_w;
        }
    }

    draw_prompt_line(0, NULL);

    cairo_surface_flush(g_cs);
    fb_blit();
    g_dirty = 0;
}

static void show_prompt(void) { g_dirty = 1; }

static void redraw_shell_line(const char *buf, int len, int cursor) {
    memcpy(g_input_buf, buf, (size_t)len); g_input_buf[len] = '\0';
    g_input_pos = len; g_input_cursor = cursor; g_dirty = 1;
}

static void hist_add(const char *line) {
    if (!*line) return;
    if (g_hist_count > 0 && u_strcmp(g_history[(g_hist_count-1) % HIST_MAX], line) == 0) return;
    u_strcpy(g_history[g_hist_count % HIST_MAX], line);
    g_hist_count++; g_hist_pos = g_hist_count;
}

struct builtin { const char *name; int (*func)(int argc, const char **argv); };
static int builtin_cd(int argc, const char **argv);
static int builtin_exit(int argc, const char **argv);
static int builtin_help(int argc, const char **argv);
static int builtin_clear(int argc, const char **argv);
static int builtin_pwd(int argc, const char **argv);
static int builtin_echo(int argc, const char **argv);
static int builtin_export(int argc, const char **argv);
static int builtin_id(int argc, const char **argv);
static int builtin_whoami(int argc, const char **argv);
static int builtin_history(int argc, const char **argv);
static int builtin_logout(int argc, const char **argv);
static int builtin_chmod(int argc, const char **argv);
static int builtin_sudo(int argc, const char **argv);
static int execute_command_argv(int argc, const char **argv);
static void maybe_emit_separator(void);

static const struct builtin builtins[] = {
    {"cd", builtin_cd}, {"clear", builtin_clear}, {"chmod", builtin_chmod},
    {"echo", builtin_echo}, {"exit", builtin_exit}, {"export", builtin_export},
    {"help", builtin_help}, {"history", builtin_history}, {"pwd", builtin_pwd},
    {"id", builtin_id}, {"sudo", builtin_sudo},
    {"whoami", builtin_whoami}, {0, 0}
};

static int find_in_path(const char *cmd, char *resolved, unsigned int size) {
    if (!cmd || !*cmd) return GOSH_ERR_NOTFOUND;
    if (cmd[0] == '/') {
        unsigned int i = 0; while (cmd[i] && i + 1 < size) { resolved[i] = cmd[i]; i++; }
        resolved[i] = '\0'; struct linux_stat64 st;
        if (sys_stat64(resolved, &st) != 0) return GOSH_ERR_NOTFOUND;
        return (sys_access(resolved, LINUX_X_OK) == 0) ? 0 : GOSH_ERR_ACCES;
    }
    const char *path = env_get("PATH"); if (!path) path = "/bin:/usr/bin";
    char path_copy[PATH_MAX]; unsigned int pi = 0;
    while (*path && pi + 1 < sizeof(path_copy)) path_copy[pi++] = *path++;
    path_copy[pi] = '\0'; int has_permission_error = 0; char *start = path_copy;
    while (*start) {
        char *end = start; while (*end && *end != ':') end++;
        char sep = *end; *end = '\0';
        unsigned int si = 0; while (start[si] && si + 1 < size) { resolved[si] = start[si]; si++; }
        if (si > 0 && resolved[si - 1] != '/') { if (si + 1 < size) resolved[si++] = '/'; }
        unsigned int ci = 0; while (cmd[ci] && si + 1 < size) { resolved[si++] = cmd[ci++]; }
        resolved[si] = '\0'; struct linux_stat64 st;
        if (sys_stat64(resolved, &st) == 0) {
            if (sys_access(resolved, LINUX_X_OK) == 0) return 0;
            has_permission_error = 1;
        }
        if (!sep) break; start = end + 1;
    }
    return has_permission_error ? GOSH_ERR_ACCES : GOSH_ERR_NOTFOUND;
}

static int is_builtin(const char *name) {
    for (const struct builtin *b = builtins; b->name; b++)
        if (u_strcmp(name, b->name) == 0) return 1;
    return 0;
}

static void redraw_shell_adapter(const char *buf, int len, int cursor, void *ctx) {
    (void)ctx; redraw_shell_line(buf, len, cursor);
}

static int read_line_common(char *buf, int max, int use_history,
    void (*redraw)(const char *, int, int, void *), void *redraw_ctx) {
    int pos = 0, cursor = 0, history_index = g_hist_count;
    char current_line_backup[LINE_MAX]; int has_backup = 0;
    if (!buf || max < 2) return -1;
    buf[0] = '\0'; current_line_backup[0] = '\0';
    redraw(buf, pos, cursor, redraw_ctx);
    display_update();
    int esc_state = 0;
    for (;;) {
        struct pollfd pfd;
        pfd.fd = 0;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int ret = poll(&pfd, 1, 100);
        if (ret < 0) return -1;
        if (ret > 0 && (pfd.revents & POLLIN)) {
            unsigned char ch;
            if (read(0, &ch, 1) == 1) {
                if (esc_state == 0 && ch == 27) { esc_state = 1; continue; }
                if (esc_state == 1) {
                    if (ch == '[' || ch == 'O') { esc_state = 2; continue; }
                    esc_state = 0; continue;
                }
                if (esc_state == 2) {
                    esc_state = 0;
                    if (ch == 'A' && use_history) {
                        if (history_index > MAX(0, g_hist_count - HIST_MAX)) {
                            if (history_index == g_hist_count) { u_strcpy(current_line_backup, buf); has_backup = 1; }
                            history_index--;
                            u_strcpy(buf, g_history[history_index % HIST_MAX]);
                            pos = (int)u_strlen(buf); cursor = pos;
                            redraw(buf, pos, cursor, redraw_ctx);
                            display_update();
                        } continue;
                    }
                    if (ch == 'B' && use_history) {
                        if (history_index < g_hist_count) {
                            history_index++;
                            if (history_index == g_hist_count) {
                                if (has_backup) u_strcpy(buf, current_line_backup);
                                else buf[0] = '\0';
                            } else { u_strcpy(buf, g_history[history_index % HIST_MAX]); }
                            pos = (int)u_strlen(buf); cursor = pos;
                            redraw(buf, pos, cursor, redraw_ctx);
                            display_update();
                        } continue;
                    }
                    if (ch == 'C') { if (cursor < pos) { cursor++; redraw(buf, pos, cursor, redraw_ctx); display_update(); } continue; }
                    if (ch == 'D') { if (cursor > 0) { cursor--; redraw(buf, pos, cursor, redraw_ctx); display_update(); } continue; }
                    if (ch == 'H' || ch == '1') { if (ch == '1') esc_state = 3; else { cursor = 0; redraw(buf, pos, cursor, redraw_ctx); display_update(); } continue; }
                    if (ch == 'F' || ch == '4') { if (ch == '4') esc_state = 3; else { cursor = pos; redraw(buf, pos, cursor, redraw_ctx); display_update(); } continue; }
                    if (ch == '3') { esc_state = 3; continue; }
                    continue;
                }
                if (esc_state == 3) {
                    esc_state = 0;
                    if (ch == '~') {
                        if (cursor < pos) {
                            for (int i = cursor; i < pos - 1; ++i) buf[i] = buf[i + 1];
                            pos--; buf[pos] = '\0';
                            redraw(buf, pos, cursor, redraw_ctx);
                            display_update();
                        }
                    } continue;
                }
                if (ch == '\n' || ch == '\r') {
                    term_newline(); buf[pos] = '\0';
                    if (use_history) hist_add(buf);
                    display_update(); return pos;
                }
                if (ch == 127 || ch == 8) {
                    if (cursor > 0) {
                        for (int i = cursor - 1; i < pos - 1; ++i) buf[i] = buf[i + 1];
                        cursor--; pos--; buf[pos] = '\0';
                        redraw(buf, pos, cursor, redraw_ctx);
                        display_update();
                    } continue;
                }
                if (ch >= 32) {
                    if (pos >= max - 1) continue;
                    if (cursor == pos) { buf[pos++] = (char)ch; cursor = pos; buf[pos] = '\0'; }
                    else {
                        for (int i = pos; i > cursor; --i) buf[i] = buf[i - 1];
                        buf[cursor] = (char)ch; pos++; cursor++; buf[pos] = '\0';
                    }
                    redraw(buf, pos, cursor, redraw_ctx);
                    display_update();
                }
            }
        } else {
            if (g_dirty) display_update();
            sys_sched_yield();
        }
    }
}

static int read_line(char *buf, int max) {
    return read_line_common(buf, max, 1, redraw_shell_adapter, NULL);
}

static void skip_ws(const char **p) { while (**p == ' ' || **p == '\t') (*p)++; }

static int parse_line(char *line, const char **argv) {
    int argc = 0; char *wp = line, *rp = line; char quote = 0;
    while (*rp) {
        skip_ws((const char **)&rp); if (!*rp) break;
        if (argc >= ARG_MAX - 1) return -1;
        argv[argc++] = wp; quote = 0;
        while (*rp) {
            char ch = *rp++;
            if (quote) {
                if (ch == quote) { quote = 0; continue; }
                if (ch == '\\' && quote == '"' && (*rp == '"' || *rp == '\\')) ch = *rp++;
                *wp++ = ch; continue;
            }
            if (ch == '"' || ch == '\'') { quote = ch; continue; }
            if (ch == ' ' || ch == '\t') break;
            if (ch == '\\' && *rp) ch = *rp++;
            *wp++ = ch;
        }
        if (quote) return -1;
        *wp++ = '\0';
    }
    argv[argc] = 0; return argc;
}

static int builtin_cd(int argc, const char **argv) {
    (void)argc;
    const char *target = argv[1] ? argv[1] : (g_session_home[0] ? g_session_home : "/");
    if (sys_chdir(target) < 0) { write_str("gosh: cd: "); write_str(target); write_str(": No such file or directory\n"); return 1; }
    return 0;
}

static int builtin_exit(int argc, const char **argv) {
    int code = 0; if (argc > 1) { int ok = 0; code = (int)u_parse_uint(argv[1], &ok); if (!ok) code = 0; }
    g_exit_code = code; g_last_exit = -1; return -1;
}

static int builtin_help(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str("GOSH! - OpenHobbyOS Shell\nBuilt-in commands:\n  cd <dir>     Change directory\n  clear        Clear the screen\n  echo <text>  Print text\n  exit [code]  Exit the shell\n  help         Display this help\n  history      Show command history\n  id           Show current identity\n  logout       Return to the login screen\n  chmod MODE PATH...  Change file mode\n  sudo CMD...  Run a command as root\n  pwd          Print working directory\n  export       Print environment variables\n");
    return 0;
}

static int builtin_clear(int argc, const char **argv) { (void)argc; (void)argv; term_clear(); g_prompt_needs_spacing = 0; return 0; }
static int builtin_pwd(int argc, const char **argv) { (void)argc; (void)argv; if (sys_getcwd(g_cwd, sizeof(g_cwd))) { write_str(g_cwd); write_ch('\n'); } return 0; }

static int builtin_echo(int argc, const char **argv) {
    for (int i = 1; i < argc; i++) { if (i > 1) write_ch(' '); write_str(argv[i]); }
    write_ch('\n'); return 0;
}

static int builtin_export(int argc, const char **argv) {
    (void)argv; if (argc == 1) { for (int i = 0; g_envp && g_envp[i]; i++) write_str(g_envp[i]), write_ch('\n'); return 0; }
    write_str("gosh: export: setting variables not supported\n"); return 1;
}

static int builtin_id(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str("uid="); u_put_uint((unsigned int)sys_getuid32());
    write_str(" gid="); u_put_uint((unsigned int)sys_getgid32());
    write_str(" euid="); u_put_uint((unsigned int)sys_geteuid32());
    write_str(" egid="); u_put_uint((unsigned int)sys_getegid32());
    write_str("\n"); return 0;
}

static int builtin_whoami(int argc, const char **argv) {
    (void)argc; (void)argv;
    write_str(g_session_user[0] ? g_session_user : "root"); write_str("\n"); return 0;
}

static int builtin_history(int argc, const char **argv) {
    (void)argc; (void)argv;
    int start = MAX(0, g_hist_count - HIST_MAX);
    for (int i = start; i < g_hist_count; i++) {
        char num[12]; unsigned int np = 0; int n = i + 1;
        char rev[8]; unsigned int rp = 0;
        while (n) { rev[rp++] = (char)('0' + (n % 10)); n /= 10; }
        while (rp) num[np++] = rev[--rp]; num[np] = '\0';
        write_str(num); write_str("  "); write_str(g_history[i % HIST_MAX]); write_ch('\n');
    }
    return 0;
}

static unsigned int parse_octal_mode(const char *text, int *ok) {
    unsigned int mode = 0; if (ok) *ok = 0;
    if (!text || !*text) return 0;
    while (*text) { if (*text < '0' || *text > '7') return 0; mode = (mode << 3) | (unsigned int)(*text - '0'); text++; }
    if (ok) *ok = 1; return mode;
}

static int builtin_chmod(int argc, const char **argv) {
    int ok = 0; unsigned int mode; int failed = 0;
    if (argc < 3) { write_str("chmod: usage: chmod MODE FILE...\n"); return 1; }
    mode = parse_octal_mode(argv[1], &ok);
    if (!ok) { write_str("chmod: invalid mode\n"); return 1; }
    for (int i = 2; i < argc; i++) { if (sys_chmod(argv[i], (int)mode) < 0) { write_str("chmod: "); write_str(argv[i]); write_str(": failed\n"); failed = 1; } }
    return failed;
}

static int builtin_sudo(int argc, const char **argv) {
    char password[64]; unsigned int old_uid, old_gid, old_euid, old_egid; int rc;
    if (argc < 2) { write_str("sudo: usage: sudo COMMAND [ARGS...]\n"); return 1; }
    if (sys_geteuid32() == 0) return execute_command_argv(argc - 1, argv + 1);
    write_str("sudo password for root: ");
    int pi = 0; while (pi < (int)sizeof(password) - 1) {
        char c; if (read(0, &c, 1) != 1) break;
        if (c == '\n' || c == '\r') break;
        if ((c == 127 || c == 8) && pi > 0) pi--;
        else if (c >= 32) password[pi++] = c;
    }
    password[pi] = '\0'; write_ch('\n');
    old_uid = (unsigned int)sys_getuid32(); old_gid = (unsigned int)sys_getgid32();
    old_euid = (unsigned int)sys_geteuid32(); old_egid = (unsigned int)sys_getegid32();
    if (sys_auth("root", password) < 0) { write_str("sudo: authentication failed\n"); return 1; }
    rc = execute_command_argv(argc - 1, argv + 1);
    sys_setuid(old_uid); sys_setgid(old_gid); sys_seteuid(old_euid); sys_setegid(old_egid);
    return rc;
}

static int run_builtin(const char *name, int argc, const char **argv) {
    for (const struct builtin *b = builtins; b->name; b++)
        if (u_strcmp(name, b->name) == 0) return b->func(argc, argv);
    return -1;
}

static int execute_external_captured(const char *path, const char **argv) {
    int p[2]; if (sys_pipe(p) < 0) { write_str("gosh: pipe failed\n"); return 1; }
    int pid = sys_fork();
    if (pid < 0) { sys_close(p[0]); sys_close(p[1]); write_str("gosh: fork failed\n"); return 1; }
    if (pid == 0) {
        sys_close(p[0]); sys_dup2(p[1], 1); sys_dup2(p[1], 2); sys_close(p[1]);
        char *exec_argv[64]; int ac = 0;
        while (argv[ac] && ac < 63) { exec_argv[ac] = (char *)argv[ac]; ac++; }
        exec_argv[ac] = NULL;
        int ee = 0; while (g_envp[ee]) ee++;
        const char *new_envp[ee + 2];
        for (int ei = 0; ei < ee; ei++) new_envp[ei] = g_envp[ei];
        new_envp[ee] = "TERM=xterm-256color";
        new_envp[ee + 1] = NULL;
        sys_execve(path, exec_argv, (char **)new_envp);
        write(2, "gosh: exec failed\n", 18); _exit(127);
    }
    sys_close(p[1]);
    char buf[4096];
    for (;;) {
        struct pollfd pf; pf.fd = p[0]; pf.events = POLLIN; pf.revents = 0;
        if (poll(&pf, 1, 100) <= 0) {
            int status = 0;
            if (sys_waitpid(pid, &status, WNOHANG) == pid) { sys_close(p[0]); return status; }
            if (g_dirty) display_update(); continue;
        }
        int n = (int)read(p[0], buf, sizeof(buf));
        if (n <= 0) break;
        term_write(buf, n); display_update();
    }
    int status = 0; sys_waitpid(pid, &status, 0); sys_close(p[0]); return status;
}

static int execute_command_argv(int argc, const char **argv) {
    if (argc <= 0 || !argv || !argv[0] || !*argv[0]) return 0;
    int r = run_builtin(argv[0], argc, argv);
    if (r >= 0) return r;
    char resolved[PATH_MAX];
    int path_result = find_in_path(argv[0], resolved, sizeof(resolved));
    if (path_result == 0) {
        unsigned int rlen = u_strlen(resolved);
        if (rlen > 6 && u_strcmp(resolved + rlen - 6, ".ohpkg") == 0) {
            const char *ohpkg_argv[ARG_MAX]; ohpkg_argv[0] = "ohpkg-run"; ohpkg_argv[1] = resolved;
            int i; for (i = 1; i < argc && i + 1 < ARG_MAX - 1; i++) ohpkg_argv[i + 1] = argv[i];
            ohpkg_argv[i + 1] = 0; char runner[PATH_MAX];
            if (find_in_path("ohpkg-run", runner, sizeof(runner)) == 0) return execute_external_captured(runner, ohpkg_argv);
            write_str("gosh: ohpkg-run: not found\n"); return 127;
        }
        return execute_external_captured(resolved, argv);
    }
    if (path_result == GOSH_ERR_ACCES || path_result == GOSH_ERR_PERM) { write_str("gosh: "); write_str(argv[0]); write_str(": permission denied\n"); return 126; }
    write_str("gosh: "); write_str(argv[0]); write_str(": command not found\n"); return 127;
}

static void execute_line(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == '#') { g_prompt_needs_spacing = 0; return; }
    const char *argv[ARG_MAX]; int argc = parse_line(line, argv);
    if (argc <= 0) { g_prompt_needs_spacing = 0; return; }
    g_prompt_needs_spacing = 1; g_last_exit = execute_command_argv(argc, argv);
}

static void maybe_emit_separator(void) { if (g_prompt_needs_spacing) { write_ch('\n'); g_prompt_needs_spacing = 0; } }

static void gosh_set_user(const char *username) {
    u_strcpy(g_session_user, username);
    if (u_strcmp(username, "root") == 0) { u_strcpy(g_session_home, "/root"); u_strcpy(g_session_shell, "/bin/gosh"); }
    else { u_strcpy(g_session_home, "/home/user"); u_strcpy(g_session_shell, "/bin/gosh"); }
    g_logged_in = 1;
    if (g_session_home[0] && sys_chdir(g_session_home) < 0) sys_chdir("/");
    g_hist_count = 0; g_hist_pos = 0; g_prompt_needs_spacing = 0;
    g_last_exit = 0;
}

static int login_screen(char *out_user, int max_user) {
    char username[64] = {0};
    char password[64] = {0};
    char error[256] = {0};
    int field = 0;
    int pos_u = 0, pos_p = 0;

    for (;;) {
        pixel_clear(0x121418);

        int cx = g_win_w / 2;
        int cy = g_win_h / 2;

        int box_w = 400;
        int box_h = 260;
        int bx = cx - box_w / 2;
        int by = cy - box_h / 2;

        pixel_fill(bx, by, box_w, box_h, 0x1A1C22);
        pixel_fill(bx, by, box_w, 1, 0x3A5CC0);
        pixel_fill(bx, by + box_h - 1, box_w, 1, 0x101218);

        text_at(cx - 90, by + 30, "Welcome to OpenHobbyOS", 0.63, 0.75, 1.0);

        text_at(bx + 30, by + 70, "Username:", 0.70, 0.73, 0.78);
        pixel_fill(bx + 30, by + 92, box_w - 60, 32, 0x26282E);
        pixel_fill(bx + 30, by + 92, box_w - 60, 1, 0x41444C);
        {
            char tmp[64];
            int ulen = pos_u < (int)sizeof(username) ? pos_u : (int)sizeof(username) - 1;
            memcpy(tmp, username, (size_t)ulen); tmp[ulen] = '\0';
            text_at(bx + 36, by + 98, tmp, 0.84, 0.85, 0.88);
            if (field == 0) {
                int cur_x = bx + 36 + ulen * g_font_w;
                pixel_fill(cur_x, by + 98, g_font_w, g_font_h, 0x8C9EFF);
            }
        }

        text_at(bx + 30, by + 136, "Password:", 0.70, 0.73, 0.78);
        pixel_fill(bx + 30, by + 158, box_w - 60, 32, 0x26282E);
        pixel_fill(bx + 30, by + 158, box_w - 60, 1, 0x41444C);
        {
            char masked[64];
            int plen = pos_p < (int)sizeof(password) ? pos_p : (int)sizeof(password) - 1;
            for (int i = 0; i < plen && i < 63; i++) masked[i] = '*';
            masked[plen] = '\0';
            text_at(bx + 36, by + 164, masked, 0.84, 0.85, 0.88);
            if (field == 1) {
                int cur_x = bx + 36 + plen * g_font_w;
                pixel_fill(cur_x, by + 164, g_font_w, g_font_h, 0x8C9EFF);
            }
        }

        if (error[0]) {
            text_at(bx + 30, by + 210, error, 0.94, 0.31, 0.29);
        }

        cairo_surface_flush(g_cs);
        fb_blit();

        struct pollfd pfd;
        pfd.fd = 0;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int ret = poll(&pfd, 1, 50);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            unsigned char ch;
            if (read(0, &ch, 1) == 1) {
                if (ch == '\t') {
                    field = field == 0 ? 1 : 0;
                } else if (ch == '\n' || ch == '\r') {
                    if (field == 0) {
                        field = 1;
                    } else {
                        int auth_ok = (sys_auth(username, password) == 0);
                        if (auth_ok) {
                            unsigned int ulen = u_strlen(username);
                            unsigned int mlen = (unsigned int)max_user - 1;
                            if (ulen > mlen) ulen = mlen;
                            memcpy(out_user, username, ulen);
                            out_user[ulen] = '\0';
                            return 0;
                        } else {
                            snprintf(error, sizeof(error), "Authentication failed");
                            password[0] = '\0';
                            pos_p = 0;
                            field = 1;
                        }
                    }
                } else if (ch == 127 || ch == 8) {
                    if (field == 0 && pos_u > 0) { username[--pos_u] = '\0'; }
                    else if (field == 1 && pos_p > 0) { password[--pos_p] = '\0'; }
                } else if (ch >= 32) {
                    if (field == 0 && pos_u < (int)sizeof(username) - 1) { username[pos_u++] = (char)ch; username[pos_u] = '\0'; }
                    else if (field == 1 && pos_p < (int)sizeof(password) - 1) { password[pos_p++] = (char)ch; password[pos_p] = '\0'; }
                }
            }
        } else {
            sys_sched_yield();
        }
    }
}

static const char *gosh_get_user(void) {
    const char *u = env_get("USER");
    if (!u) u = env_get("LOGNAME");
    if (!u) u = "root";
    return u;
}

static int builtin_logout(int argc, const char **argv) {
    (void)argc; (void)argv;
    g_logged_in = 0;
    g_session_user[0] = '\0';
    return -1;
}

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv;
    g_envp = envp; g_last_exit = 0; g_exit_code = 0; g_hist_count = 0; g_hist_pos = 0;
    load_theme(); detect_color_support();

    if (display_init() < 0) return 1;

    const char *env_user = gosh_get_user();
    if (env_user && *env_user && u_strcmp(env_user, "root") != 0) {
        gosh_set_user(env_user);
    }

    for (;;) {
        if (!g_logged_in) {
            char login_user[32] = {0};
            login_screen(login_user, sizeof(login_user));
            gosh_set_user(login_user);
            term_clear();
            write_str("GOSH! v2.0 - OpenHobbyOS Shell\n");
            term_write("Type 'help' for available commands.\n", 37);
            display_update();
        }

        char line[LINE_MAX];
        for (;;) {
            maybe_emit_separator(); show_prompt();
            int len = read_line(line, sizeof(line));
            if (len < 0) break;
            execute_line(line);
            if (g_last_exit < 0) break;
        }
        if (g_exit_code == 0 && g_last_exit == -1 && g_logged_in) {
            return 0;
        }
    }
}
