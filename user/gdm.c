#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>

void _exit(int status) __attribute__((noreturn));

#define LINUX_SYS_OPEN   5
#define LINUX_SYS_CLOSE  6
#define LINUX_SYS_IOCTL 54
#define LINUX_SYS_MMAP2 192
#define LINUX_SYS_MUNMAP 91
#define LINUX_SYS_FORK   2
#define LINUX_SYS_EXECVE 11
#define LINUX_SYS_EXIT_GROUP 252
#define LINUX_SYS_CHDIR  12
#define OHOS_SYS_WAITPID 420
#define OHOS_SYS_AUTH    423
#define OHOS_SYS_YIELD   410

#define FBIOGET_VSCREENINFO 0x4600

static inline int syscall3(int n, int a, int b, int c) {
    int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}

static inline int syscall2(int n, int a, int b) {
    int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b) : "memory");
    return r;
}

static inline int syscall1(int n, int a) {
    int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "b"(a) : "memory");
    return r;
}

static inline int syscall0(int n) {
    int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n) : "memory");
    return r;
}

static inline void *syscall6(int n, int a, int b, int c, int d, int e, int f) {
    void *r;
    __asm__ volatile ("push %%ebp\nmov %7, %%ebp\nint $0x80\npop %%ebp"
        : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e), "r"(f) : "memory");
    return r;
}

static int ohs_open(const char *p, int fl, int mo) { return syscall3(LINUX_SYS_OPEN, (int)p, fl, mo); }
static int ohs_close(int fd) { return syscall1(LINUX_SYS_CLOSE, fd); }
static int ohs_ioctl(int fd, unsigned int rq, void *ap) { return syscall3(LINUX_SYS_IOCTL, fd, (int)rq, (int)ap); }
static void *ohs_mmap(void *a, unsigned int l, int p, int f, int fd, unsigned int po) {
    return syscall6(LINUX_SYS_MMAP2, (int)a, (int)l, p, f, fd, (int)po);
}
static int ohs_munmap(void *a, unsigned int l) { return syscall2(LINUX_SYS_MUNMAP, (int)a, (int)l); }
static int ohs_fork(void) { return syscall0(LINUX_SYS_FORK); }
static int ohs_execve(const char *p, char **a, char **e) { return syscall3(LINUX_SYS_EXECVE, (int)p, (int)a, (int)e); }
static int ohs_waitpid(int p, int *s, int o) { return syscall3(OHOS_SYS_WAITPID, p, (int)s, o); }
static int ohs_auth(const char *u, const char *p) { return syscall2(OHOS_SYS_AUTH, (int)u, (int)p); }
static int ohs_yield(void) { return syscall0(OHOS_SYS_YIELD); }
static int ohs_chdir(const char *p) { return syscall1(LINUX_SYS_CHDIR, (int)p); }

struct fb_bitfield {
    unsigned int offset, length, msb_right;
};

struct fb_var_screeninfo {
    unsigned int xres, yres, xres_virtual, yres_virtual;
    unsigned int xoffset, yoffset, bits_per_pixel, grayscale;
    struct fb_bitfield red, green, blue, transp;
    unsigned int pitch;
};

static int g_fb_fd = -1;
static unsigned char *g_fb_map;
static int g_fb_w, g_fb_h;
static unsigned int *g_pixels;
static int g_mouse_fd = -1;
static int g_mouse_x, g_mouse_y;

static struct nk_context g_ctx;
static struct nk_font_atlas g_atlas;

static int g_auth_ok;
static char g_username[64];
static char g_password[64];
static char g_error[256];

static int fb_init(void) {
    g_fb_fd = ohs_open("/dev/fb0", 2, 0);
    if (g_fb_fd < 0) return -1;

    struct fb_var_screeninfo vi;
    memset(&vi, 0, sizeof(vi));
    ohs_ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &vi);

    g_fb_w = vi.xres;
    g_fb_h = vi.yres;

    int bpp = vi.bits_per_pixel;
    unsigned int fb_size = (unsigned int)(g_fb_w * g_fb_h * ((bpp + 7) / 8));
    g_fb_map = (unsigned char *)ohs_mmap(NULL, fb_size, 3, 1, g_fb_fd, 0);
    if (!g_fb_map || g_fb_map == (void *)-1) {
        ohs_close(g_fb_fd);
        g_fb_fd = -1;
        return -1;
    }

    g_pixels = (unsigned int *)malloc((size_t)(g_fb_w * g_fb_h * 4));
    if (!g_pixels) return -1;
    memset(g_pixels, 0, (size_t)(g_fb_w * g_fb_h * 4));
    return 0;
}

static void fb_blit(void) {
    for (int y = 0; y < g_fb_h; y++) {
        unsigned char *dst = g_fb_map + y * g_fb_w * 3;
        unsigned int *src = g_pixels + y * g_fb_w;
        for (int x = 0; x < g_fb_w; x++) {
            dst[x * 3]     = (unsigned char)(src[x] >> 16);
            dst[x * 3 + 1] = (unsigned char)(src[x] >> 8);
            dst[x * 3 + 2] = (unsigned char)(src[x]);
        }
    }
}

static int mouse_init(void) {
    g_mouse_fd = ohs_open("/dev/mouse", 2, 0);
    if (g_mouse_fd < 0) return -1;
    g_mouse_x = g_fb_w / 2;
    g_mouse_y = g_fb_h / 2;
    return 0;
}

static void mouse_poll(struct nk_context *ctx) {
    if (g_mouse_fd < 0) return;
    struct pollfd mpfd;
    mpfd.fd = g_mouse_fd;
    mpfd.events = POLLIN;
    mpfd.revents = 0;
    while (poll(&mpfd, 1, 0) > 0 && (mpfd.revents & POLLIN)) {
        unsigned char buf[3];
        int n = (int)read(g_mouse_fd, buf, 3);
        if (n < 3) break;
        g_mouse_x += (int)((char)buf[1]);
        g_mouse_y -= (int)((char)buf[2]);
        if (g_mouse_x < 0) g_mouse_x = 0;
        if (g_mouse_x >= g_fb_w) g_mouse_x = g_fb_w - 1;
        if (g_mouse_y < 0) g_mouse_y = 0;
        if (g_mouse_y >= g_fb_h) g_mouse_y = g_fb_h - 1;
        unsigned char btn = buf[0] & 0x07;
        nk_input_motion(ctx, g_mouse_x, g_mouse_y);
        nk_input_button(ctx, NK_BUTTON_LEFT, g_mouse_x, g_mouse_y, (btn & 1) != 0);
        nk_input_button(ctx, NK_BUTTON_RIGHT, g_mouse_x, g_mouse_y, (btn & 2) != 0);
    }
}

static void draw_fill_rect(unsigned int *buf, short x, short y, short w, short h, struct nk_color c) {
    unsigned int color = ((unsigned int)c.r << 16) | ((unsigned int)c.g << 8) | (unsigned int)c.b;
    for (short row = y < 0 ? 0 : y; row < y + h && row < g_fb_h; row++) {
        if (row < 0) continue;
        for (short col = x < 0 ? 0 : x; col < x + w && col < g_fb_w; col++) {
            if (col < 0) continue;
            buf[row * g_fb_w + col] = color;
        }
    }
}

static void blend_glyph(unsigned int *buf, int dst_x, int dst_y,
    const unsigned char *atlas, int aw, int ah,
    float u0, float v0, float u1, float v1,
    unsigned int fg)
{
    int gx = (int)(u0 * aw);
    int gy = (int)(v0 * ah);
    int gw = (int)((u1 - u0) * aw);
    int gh = (int)((v1 - v0) * ah);
    if (gw < 1) gw = 1;
    if (gh < 1) gh = 1;

    unsigned char sr = (unsigned char)(fg >> 16);
    unsigned char sg = (unsigned char)(fg >> 8);
    unsigned char sb = (unsigned char)(fg);

    for (int row = 0; row < gh; row++) {
        int py = dst_y + row;
        if (py < 0 || py >= g_fb_h) continue;
        for (int col = 0; col < gw; col++) {
            int px = dst_x + col;
            if (px < 0 || px >= g_fb_w) continue;
            unsigned char a = atlas[(gy + row) * aw + (gx + col)];
            if (a == 0) continue;
            unsigned int bg = buf[py * g_fb_w + px];
            if (a == 255) {
                buf[py * g_fb_w + px] = fg;
            } else {
                unsigned char br = (unsigned char)(bg >> 16);
                unsigned char bg_ = (unsigned char)(bg >> 8);
                unsigned char bb = (unsigned char)(bg);
                buf[py * g_fb_w + px] =
                    ((unsigned int)(((int)sr * a + (int)br * (255 - a)) / 255) << 16) |
                    ((unsigned int)(((int)sg * a + (int)bg_ * (255 - a)) / 255) << 8) |
                    ((unsigned int)(((int)sb * a + (int)bb * (255 - a)) / 255));
            }
        }
    }
}

static void gdm_render(void) {
    const struct nk_command *cmd = nk__begin(&g_ctx);
    while (cmd) {
        switch (cmd->type) {
        case NK_COMMAND_NOP: break;
        case NK_COMMAND_SCISSOR: break;
        case NK_COMMAND_RECT_FILLED: {
            const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
            draw_fill_rect(g_pixels, r->x, r->y, r->w, r->h, r->color);
            break;
        }
        case NK_COMMAND_TEXT: {
            const struct nk_command_text *t = (const struct nk_command_text *)cmd;
            const unsigned char *str = (const unsigned char *)t->string;
            int len = t->length;
            int pen_x = t->x;
            int pen_y = t->y;
            unsigned int fg = ((unsigned int)t->foreground.r << 16) |
                              ((unsigned int)t->foreground.g << 8) |
                              (unsigned int)t->foreground.b;
            const struct nk_user_font *f = t->font;
            const unsigned char *ap = (const unsigned char *)g_atlas.pixel;
            int aw = g_atlas.tex_width;
            int ah = g_atlas.tex_height;

            for (int i = 0; i < len;) {
                nk_rune cp;
                i += nk_utf_decode(str + i, &cp, len - i);
                if (cp == '\n') { pen_x = t->x; pen_y += (int)f->height; continue; }
                if (cp < 32) continue;

                struct nk_user_font_glyph g;
                memset(&g, 0, sizeof(g));
                f->query(f->userdata, f->height, &g, cp, 1);

                int gx = pen_x + (int)g.offset.x;
                int gy = pen_y + (int)g.offset.y;

                blend_glyph(g_pixels, gx, gy, ap, aw, ah,
                    g.uv[0].x, g.uv[0].y, g.uv[1].x, g.uv[1].y, fg);
                pen_x += (int)g.xadvance;
            }
            break;
        }
        default: break;
        }
        cmd = nk__next(&g_ctx, cmd);
    }
}

static void handle_keyboard(struct nk_context *ctx) {
    unsigned char ch;
    static int esc = 0;
    if (read(0, &ch, 1) != 1) return;

    if (esc == 0 && ch == 27) { esc = 1; return; }
    if (esc == 1) {
        if (ch == '[' || ch == 'O') { esc = 2; return; }
        esc = 0; return;
    }
    if (esc == 2) {
        esc = 0;
        switch (ch) {
        case 'A': nk_input_key(ctx, NK_KEY_UP, 1); return;
        case 'B': nk_input_key(ctx, NK_KEY_DOWN, 1); return;
        case 'C': nk_input_key(ctx, NK_KEY_RIGHT, 1); return;
        case 'D': nk_input_key(ctx, NK_KEY_LEFT, 1); return;
        case 'H': nk_input_key(ctx, NK_KEY_TEXT_START, 1); return;
        case 'F': nk_input_key(ctx, NK_KEY_TEXT_END, 1); return;
        case '3': esc = 3; return;
        }
        return;
    }
    if (esc == 3) { esc = 0; if (ch == '~') nk_input_key(ctx, NK_KEY_DEL, 1); return; }

    if (ch == '\n' || ch == '\r') nk_input_key(ctx, NK_KEY_ENTER, 1);
    else if (ch == 127 || ch == 8) nk_input_key(ctx, NK_KEY_BACKSPACE, 1);
    else if (ch == 9) nk_input_key(ctx, NK_KEY_TAB, 1);
    else if (ch >= 32) nk_input_unicode(ctx, (nk_rune)ch);
}

static void login_ui(struct nk_context *ctx) {
    if (nk_begin(ctx, "GDM", nk_rect(0, 0, (float)g_fb_w, (float)g_fb_h),
        NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND))
    {
        nk_style_push_style_item(ctx, &ctx->style.window.fixed_background,
            nk_style_item_color(nk_rgb(22, 24, 28)));

        nk_layout_row_dynamic(ctx, 60, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 48, 1);
        nk_label_colored(ctx, "Welcome to OpenHobbyOS",
            NK_TEXT_CENTERED, nk_rgb(160, 190, 255));

        nk_layout_row_dynamic(ctx, 10, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 42, 1);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
            g_username, sizeof(g_username), nk_filter_ascii);

        nk_layout_row_dynamic(ctx, 10, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 42, 1);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
            g_password, sizeof(g_password), nk_filter_ascii);

        nk_layout_row_dynamic(ctx, 14, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 44, 1);
        if (nk_button_label(ctx, "Sign In")) {
            if (ohs_auth(g_username, g_password) == 0) {
                g_auth_ok = 1;
            } else {
                snprintf(g_error, sizeof(g_error),
                    "Authentication failed for '%s'", g_username);
                g_password[0] = '\0';
            }
        }

        if (g_error[0]) {
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label_colored(ctx, g_error,
                NK_TEXT_CENTERED, nk_rgb(240, 80, 80));
        }

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_colored(ctx, "Available accounts: root, user",
            NK_TEXT_CENTERED, nk_rgb(90, 92, 100));

        nk_style_pop_style_item(ctx);
    }
    nk_end(ctx);
}

static void auth_and_exec(void) {
    const char *shell = "/bin/gosh";
    const char *home = strcmp(g_username, "root") == 0 ? "/root" : "/home/user";
    ohs_chdir(home);

    const char *argv[] = {shell, NULL};
    char envp[4][64];
    snprintf(envp[0], sizeof(envp[0]), "TERM=xterm-256color");
    snprintf(envp[1], sizeof(envp[1]), "HOME=%s", home);
    snprintf(envp[2], sizeof(envp[2]), "USER=%s", g_username);
    snprintf(envp[3], sizeof(envp[3]), "SHELL=%s", shell);

    const char *envp_ptr[] = {envp[0], envp[1], envp[2], envp[3], NULL};

    int pid = ohs_fork();
    if (pid < 0) return;

    if (pid == 0) {
        ohs_execve(shell, (char **)argv, (char **)envp_ptr);
        _exit(127);
    }

    int status;
    ohs_waitpid(pid, &status, 0);
}

static void reset_state(void) {
    g_auth_ok = 0;
    g_error[0] = '\0';
    g_password[0] = '\0';
    g_username[0] = '\0';
}

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;

    if (fb_init() < 0) return 1;
    mouse_init();

    nk_font_atlas_init_default(&g_atlas);
    nk_font_atlas_begin(&g_atlas);
    struct nk_font *font = nk_font_atlas_add_from_file(&g_atlas,
        "/fonts/Monospace.ttf", 16.0f, 0);
    if (!font)
        font = nk_font_atlas_add_default(&g_atlas, 16.0f, 0);
    (void)font;
    {
        int w, h;
        nk_font_atlas_bake(&g_atlas, &w, &h, NK_FONT_ATLAS_ALPHA8);
        g_atlas.tex_width = w;
        g_atlas.tex_height = h;
    }
    nk_font_atlas_end(&g_atlas, nk_handle_id(0), NULL);

    nk_init_default(&g_ctx, &g_atlas.default_font->handle);

    g_ctx.style.window.background = nk_rgb(18, 20, 24);
    g_ctx.style.window.fixed_background = nk_style_item_color(nk_rgb(18, 20, 24));
    g_ctx.style.window.border = 0;
    g_ctx.style.window.padding = nk_vec2(40, 20);
    g_ctx.style.window.spacing = nk_vec2(0, 0);

    g_ctx.style.text.color = nk_rgb(200, 202, 210);

    g_ctx.style.button.normal = nk_style_item_color(nk_rgb(55, 95, 195));
    g_ctx.style.button.hover = nk_style_item_color(nk_rgb(70, 115, 215));
    g_ctx.style.button.active = nk_style_item_color(nk_rgb(40, 75, 170));
    g_ctx.style.button.text_normal = nk_rgb(255, 255, 255);
    g_ctx.style.button.text_hover = nk_rgb(255, 255, 255);
    g_ctx.style.button.text_active = nk_rgb(255, 255, 255);
    g_ctx.style.button.border = 0;
    g_ctx.style.button.rounding = 6.0f;
    g_ctx.style.button.padding = nk_vec2(10, 6);

    g_ctx.style.edit.normal = nk_style_item_color(nk_rgb(38, 40, 46));
    g_ctx.style.edit.hover = nk_style_item_color(nk_rgb(42, 44, 50));
    g_ctx.style.edit.active = nk_style_item_color(nk_rgb(46, 48, 54));
    g_ctx.style.edit.border_color = nk_rgb(65, 68, 76);
    g_ctx.style.edit.border = 1;
    g_ctx.style.edit.rounding = 4.0f;
    g_ctx.style.edit.cursor_normal = nk_rgb(160, 190, 255);
    g_ctx.style.edit.text_normal = nk_rgb(215, 217, 225);
    g_ctx.style.edit.padding = nk_vec2(10, 6);
    g_ctx.style.edit.row_padding = 0;

    g_ctx.style.window.header.normal = nk_style_item_color(nk_rgb(18, 20, 24));
    g_ctx.style.window.header.label_normal = nk_rgb(200, 202, 210);

    for (;;) {
        nk_input_begin(&g_ctx);

        struct pollfd pfd;
        pfd.fd = 0;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 30) > 0 && (pfd.revents & POLLIN))
            handle_keyboard(&g_ctx);

        mouse_poll(&g_ctx);
        nk_input_end(&g_ctx);

        memset(g_pixels, 0, (size_t)(g_fb_w * g_fb_h * 4));
        login_ui(&g_ctx);
        gdm_render();

        {
            unsigned int cursor = 0xCCCCCCCC;
            int cs = 6;
            for (int dy = -cs; dy <= cs; dy++) {
                for (int dx = -cs; dx <= cs; dx++) {
                    if (dx * dx + dy * dy <= cs * cs) {
                        int cx = g_mouse_x + dx;
                        int cy = g_mouse_y + dy;
                        if (cx >= 0 && cx < g_fb_w && cy >= 0 && cy < g_fb_h)
                            g_pixels[cy * g_fb_w + cx] = cursor;
                    }
                }
            }
        }

        fb_blit();
        ohs_yield();

        if (g_auth_ok) {
            nk_free(&g_ctx);
            nk_font_atlas_clear(&g_atlas);
            auth_and_exec();
            nk_font_atlas_init_default(&g_atlas);
            nk_font_atlas_begin(&g_atlas);
            struct nk_font *f2 = nk_font_atlas_add_from_file(&g_atlas,
                "/fonts/Monospace.ttf", 16.0f, 0);
            if (!f2) f2 = nk_font_atlas_add_default(&g_atlas, 16.0f, 0);
            (void)f2;
            {
                int w, h;
                nk_font_atlas_bake(&g_atlas, &w, &h, NK_FONT_ATLAS_ALPHA8);
                g_atlas.tex_width = w;
                g_atlas.tex_height = h;
            }
            nk_font_atlas_end(&g_atlas, nk_handle_id(0), NULL);
            nk_init_default(&g_ctx, &g_atlas.default_font->handle);
            reset_state();
        }
    }

    nk_free(&g_ctx);
    nk_font_atlas_clear(&g_atlas);
    if (g_mouse_fd >= 0) ohs_close(g_mouse_fd);
    ohs_munmap(g_fb_map, (size_t)(g_fb_w * g_fb_h * 3));
    ohs_close(g_fb_fd);
    free(g_pixels);
    return 0;
}
