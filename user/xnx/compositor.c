#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pixman.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "protocol.h"

/* ─── constants ─────────────────────────────────────────────────────── */
#define MAX_CLIENTS             32
#define MAX_SURFACES            256
#define MAX_SURFACES_PER_CLIENT 16
#define READ_BUF_SIZE           XNX_MAX_MESSAGE_BYTES
#define FRAME_INTERVAL_MS       16

#define FONT_PATH       "/fonts/Monospace.ttf"
#define FONT_SIZE       13
#define FONT_SIZE_TITLE 12

#define TITLE_BAR_H    26
#define BORDER_W       2
#define CLOSE_BTN_SIZE 14
#define MIN_BTN_SIZE   14
#define MAX_BTN_SIZE   14
#define RESIZE_HANDLE  6

/* colours (ARGB) */
#define COL_DESKTOP     0xFF0D1117
#define COL_TITLE_BG    0xE021262D
#define COL_TITLE_FG    0xFFC9D1D9
#define COL_BORDER      0xFF30363D
#define COL_BORDER_FOC  0xFF58A6FF
#define COL_CLOSE_BTN   0xFFDA3633
#define COL_CLOSE_HOVER 0xFFF85149
#define COL_MIN_BTN     0xFF3FB950
#define COL_MAX_BTN     0xFF58A6FF
#define COL_TEXT_DIM    0xFF8B949E
#define COL_WHITE       0xFFFFFFFF

/* ─── types ─────────────────────────────────────────────────────────── */
struct xnx_surface {
    uint32_t id;
    int client_fd;
    pixman_image_t *image;
    int32_t x, y;
    uint32_t width, height;
    char title[64];
    int z_order;
    int visible;
    int focused;
    int maximized;
    int minimized;
    int prev_x, prev_y, prev_w, prev_h;
};

struct xnx_client {
    int fd;
    uint32_t id;
    int num_surfaces;
    uint32_t surface_ids[MAX_SURFACES_PER_CLIENT];
    uint8_t read_buf[READ_BUF_SIZE];
    int read_offset;
    int msg_state;
    struct xnx_header current_header;
    int active;
};

/* ─── globals ───────────────────────────────────────────────────────── */
static struct xnx_surface surfaces[MAX_SURFACES];
static int num_surfaces;
static uint32_t next_surface_id;
static int next_z_order = 1;

static struct xnx_client clients[MAX_CLIENTS];
static int num_clients;
static uint32_t next_client_id;

static int listener_fd = -1;
static int fb_fd = -1;
static pixman_image_t *framebuffer;
static int fb_width, fb_height;
static size_t fb_size;
static uint32_t *fb_hw_pixels;
static size_t fb_hw_pitch;
static int fb_bpp;
static uint32_t focused_surface_id;

static int mouse_fd = -1;
static int pointer_x, pointer_y;
static int cursor_visible = 1;
static uint8_t prev_buttons = 0;

/* drag state */
static int drag_surface_id = -1;
static int drag_offset_x, drag_offset_y;
static int resize_surface_id = 0;
static int resize_edge = 0;
static int resize_orig_x, resize_orig_y, resize_orig_w, resize_orig_h;

/* cairo */
static cairo_surface_t *crs;
static cairo_t *cr;
static cairo_font_face_t *cr_font;
static int font_ascent, font_height, font_w;
static FT_Library ft_lib;
static FT_Face ft_face;
static char font_data[512 * 1024];
static int font_data_len;

/* pixel buffer for cairo */
static uint32_t *de_pixels;

/* ─── forward decls ─────────────────────────────────────────────────── */
static void draw_frame(void);

/* ─── helpers ───────────────────────────────────────────────────────── */
static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void flush_fb(void) {
    if (!fb_hw_pixels || !de_pixels || !fb_hw_pitch) return;
    for (int y = 0; y < fb_height; ++y) {
        uint8_t *hw = (uint8_t *)fb_hw_pixels + (size_t)y * fb_hw_pitch;
        uint8_t *sw = (uint8_t *)&de_pixels[y * fb_width];
        for (int x = 0; x < fb_width; ++x) {
            hw[x*3]   = sw[x*4];
            hw[x*3+1] = sw[x*4+1];
            hw[x*3+2] = sw[x*4+2];
        }
    }
}

static void draw_cursor(void) {
    if (!cursor_visible || !de_pixels || mouse_fd < 0) return;
    int cx = pointer_x, cy = pointer_y;

    /* arrow cursor via Cairo */
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    cairo_move_to(cr, (double)cx, (double)cy);
    cairo_line_to(cr, (double)cx, (double)(cy + 16));
    cairo_line_to(cr, (double)(cx + 5), (double)(cy + 12));
    cairo_line_to(cr, (double)(cx + 9), (double)(cy + 18));
    cairo_line_to(cr, (double)(cx + 12), (double)(cy + 16));
    cairo_line_to(cr, (double)(cx + 7), (double)(cy + 10));
    cairo_line_to(cr, (double)(cx + 12), (double)(cy + 7));
    cairo_close_path(cr);
    cairo_fill(cr);

    /* white outline */
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, (double)cx, (double)cy);
    cairo_line_to(cr, (double)cx, (double)(cy + 16));
    cairo_line_to(cr, (double)(cx + 5), (double)(cy + 12));
    cairo_line_to(cr, (double)(cx + 9), (double)(cy + 18));
    cairo_line_to(cr, (double)(cx + 12), (double)(cy + 16));
    cairo_line_to(cr, (double)(cx + 7), (double)(cy + 10));
    cairo_line_to(cr, (double)(cx + 12), (double)(cy + 7));
    cairo_close_path(cr);
    cairo_stroke(cr);
}

/* ─── send helpers ──────────────────────────────────────────────────── */
static int send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t rem = len;
    while (rem > 0) {
        ssize_t s = send(fd, p, rem, 0);
        if (s > 0) { p += (size_t)s; rem -= (size_t)s; continue; }
        if (s == 0) { errno = EPIPE; return -1; }
        if (errno == EINTR) continue;
        if (errno == EAGAIN) { usleep(1000); continue; }
        return -1;
    }
    return 0;
}

static int send_msg(int fd, uint32_t type, const void *payload, uint32_t sz) {
    struct xnx_header h = { .type = type, .payload_size = sz };
    if (send_all(fd, &h, sizeof(h)) < 0) return -1;
    if (sz && send_all(fd, payload, sz) < 0) return -1;
    return 0;
}

/* ─── surface management ────────────────────────────────────────────── */
static struct xnx_surface *find_surf(uint32_t id) {
    for (int i = 0; i < num_surfaces; ++i)
        if (surfaces[i].id == id) return &surfaces[i];
    return NULL;
}

static void ensure_focus(void) {
    struct xnx_surface *f = find_surf(focused_surface_id);
    if (f && f->visible && f->image && !f->minimized) return;
    struct xnx_surface *best = NULL;
    for (int i = 0; i < num_surfaces; ++i) {
        if (!surfaces[i].visible || !surfaces[i].image || surfaces[i].minimized) continue;
        if (!best || surfaces[i].z_order > best->z_order) best = &surfaces[i];
    }
    focused_surface_id = best ? best->id : 0;
    for (int i = 0; i < num_surfaces; ++i)
        surfaces[i].focused = (surfaces[i].id == focused_surface_id);
}

static void focus_surf(struct xnx_surface *s) {
    if (!s) { ensure_focus(); return; }
    s->z_order = next_z_order++;
    focused_surface_id = s->id;
    for (int i = 0; i < num_surfaces; ++i)
        surfaces[i].focused = (surfaces[i].id == focused_surface_id);
}

static void drop_surf(uint32_t id) {
    for (int i = 0; i < num_surfaces; ++i) {
        if (surfaces[i].id != id) continue;
        if (surfaces[i].image) pixman_image_unref(surfaces[i].image);
        surfaces[i] = surfaces[--num_surfaces];
        break;
    }
    if (focused_surface_id == id) focused_surface_id = 0;
    ensure_focus();
}

static void remove_client(int idx) {
    struct xnx_client *cl = &clients[idx];
    for (int i = 0; i < cl->num_surfaces; ++i) drop_surf(cl->surface_ids[i]);
    if (cl->fd >= 0) close(cl->fd);
    memset(cl, 0, sizeof(*cl));
    clients[idx] = clients[--num_clients];
}

static struct xnx_client *add_client(int fd) {
    if (num_clients >= MAX_CLIENTS) return NULL;
    struct xnx_client *cl = &clients[num_clients++];
    memset(cl, 0, sizeof(*cl));
    cl->fd = fd; cl->id = ++next_client_id; cl->active = 1;
    return cl;
}

static int create_surf(struct xnx_client *cl, uint32_t w, uint32_t h, uint32_t *out) {
    if (!cl || !out) return -1;
    if (cl->num_surfaces >= MAX_SURFACES_PER_CLIENT || num_surfaces >= MAX_SURFACES) return -1;
    if (w == 0 || h == 0 || w > 4096 || h > 4096) return -1;
    pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h, NULL, 0);
    if (!img) return -1;
    uint32_t id = ++next_surface_id;
    struct xnx_surface *s = &surfaces[num_surfaces++];
    memset(s, 0, sizeof(*s));
    s->id = id; s->client_fd = cl->fd; s->image = img;
    s->width = w; s->height = h; s->visible = 1;
    s->x = (fb_width - (int)w) / 2;
    s->y = (fb_height - (int)h) / 2;
    cl->surface_ids[cl->num_surfaces++] = id;
    *out = id;
    focus_surf(s);
    return 0;
}

/* ─── cairo rendering ──────────────────────────────────────────────── */
static void cairo_init(void) {
    crs = cairo_image_surface_create_for_data(
        (unsigned char *)de_pixels, CAIRO_FORMAT_ARGB32,
        fb_width, fb_height, fb_width * 4);
    cr = cairo_create(crs);

    memset(font_data, 0, sizeof(font_data));
    int fd = open(FONT_PATH, O_RDONLY);
    if (fd >= 0) {
        for (;;) {
            int n = read(fd, font_data + font_data_len, (unsigned)(sizeof(font_data) - (size_t)font_data_len));
            if (n <= 0) break;
            font_data_len += n;
            if (font_data_len >= (int)sizeof(font_data)) break;
        }
        close(fd);
    }

    if (FT_Init_FreeType(&ft_lib) == 0 && font_data_len > 0) {
        if (FT_New_Memory_Face(ft_lib, (const FT_Byte *)font_data, (FT_Long)font_data_len, 0, &ft_face) == 0) {
            FT_Set_Pixel_Sizes(ft_face, 0, FONT_SIZE);
            cr_font = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
            cairo_set_font_face(cr, cr_font);
            cairo_set_font_size(cr, FONT_SIZE);
            cairo_font_extents_t fe;
            cairo_font_extents(cr, &fe);
            font_ascent = (int)(fe.ascent + 0.5);
            font_height = (int)(fe.height + 0.5);
            cairo_text_extents_t te;
            cairo_text_extents(cr, "W", &te);
            font_w = (int)(te.x_advance + 0.5);
            if (font_w < 1) font_w = FONT_SIZE / 2;
        }
    }
}

static void cairo_rgba(uint32_t col, double *r, double *g, double *b, double *a) {
    *a = ((col >> 24) & 0xFF) / 255.0;
    *r = ((col >> 16) & 0xFF) / 255.0;
    *g = ((col >> 8)  & 0xFF) / 255.0;
    *b = (col & 0xFF) / 255.0;
}

static void draw_rect(int x, int y, int w, int h, uint32_t col) {
    double r, g, b, a;
    cairo_rgba(col, &r, &g, &b, &a);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_rectangle(cr, (double)x, (double)y, (double)w, (double)h);
    cairo_fill(cr);
}

static void draw_rect_outline(int x, int y, int w, int h, uint32_t col, int lw) {
    double r, g, b, a;
    cairo_rgba(col, &r, &g, &b, &a);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_line_width(cr, (double)lw);
    cairo_rectangle(cr, (double)x + 0.5, (double)y + 0.5, (double)(w - 1), (double)(h - 1));
    cairo_stroke(cr);
}

static void draw_rounded_rect(int x, int y, int w, int h, int rad, uint32_t col) {
    double r, g, b, a;
    cairo_rgba(col, &r, &g, &b, &a);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_new_sub_path(cr);
    cairo_arc(cr, (double)(x + w - rad), (double)(y + rad), (double)rad, -M_PI/2, 0);
    cairo_arc(cr, (double)(x + w - rad), (double)(y + h - rad), (double)rad, 0, M_PI/2);
    cairo_arc(cr, (double)(x + rad), (double)(y + h - rad), (double)rad, M_PI/2, M_PI);
    cairo_arc(cr, (double)(x + rad), (double)(y + rad), (double)rad, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_fill(cr);
}

static void draw_shadow(int x, int y, int w, int h, int radius) {
    for (int i = radius; i > 0; --i) {
        double a = 0.12 * (1.0 - (double)i / (double)radius);
        cairo_set_source_rgba(cr, 0, 0, 0, a);
        cairo_rectangle(cr, (double)(x - i), (double)(y - i),
                        (double)(w + 2*i), (double)(h + 2*i));
        cairo_fill(cr);
    }
}

static void draw_text(int x, int y, const char *text, uint32_t col) {
    if (!text || !text[0]) return;
    double r, g, b, a;
    cairo_rgba(col, &r, &g, &b, &a);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_move_to(cr, (double)x, (double)(y + font_ascent));
    cairo_show_text(cr, text);
}

static void draw_text_centered(int x, int y, int w, int h, const char *text, uint32_t col) {
    if (!text) return;
    cairo_text_extents_t te;
    cairo_text_extents(cr, text, &te);
    int tx = x + (w - (int)te.x_advance) / 2;
    int ty = y + (h - font_height) / 2;
    draw_text(tx, ty, text, col);
}

static int text_width(const char *text) {
    if (!text || !text[0]) return 0;
    cairo_text_extents_t te;
    cairo_text_extents(cr, text, &te);
    return (int)te.x_advance;
}

/* ─── hit testing ───────────────────────────────────────────────────── */
static int hit_test_titlebar(struct xnx_surface *s, int mx, int my) {
    int tb_x = s->x - BORDER_W;
    int tb_y = s->y - TITLE_BAR_H - BORDER_W;
    int tb_w = (int)s->width + 2 * BORDER_W;
    return (mx >= tb_x && mx < tb_x + tb_w && my >= tb_y && my < tb_y + TITLE_BAR_H);
}

static int hit_test_close(struct xnx_surface *s, int mx, int my) {
    int cb_x = s->x - BORDER_W + (int)s->width + 2*BORDER_W - BORDER_W - CLOSE_BTN_SIZE - 6;
    int cb_y = s->y - TITLE_BAR_H - BORDER_W + (TITLE_BAR_H - CLOSE_BTN_SIZE) / 2;
    return (mx >= cb_x && mx < cb_x + CLOSE_BTN_SIZE && my >= cb_y && my < cb_y + CLOSE_BTN_SIZE);
}

static int hit_test_min(struct xnx_surface *s, int mx, int my) {
    int mb_x = s->x - BORDER_W + (int)s->width + 2*BORDER_W - BORDER_W - CLOSE_BTN_SIZE - 6 - MIN_BTN_SIZE - 4;
    int mb_y = s->y - TITLE_BAR_H - BORDER_W + (TITLE_BAR_H - MIN_BTN_SIZE) / 2;
    return (mx >= mb_x && mx < mb_x + MIN_BTN_SIZE && my >= mb_y && my < mb_y + MIN_BTN_SIZE);
}

static int hit_test_max(struct xnx_surface *s, int mx, int my) {
    int xb_x = s->x - BORDER_W + (int)s->width + 2*BORDER_W - BORDER_W - CLOSE_BTN_SIZE - 6 - 2*(MIN_BTN_SIZE + 4);
    int xb_y = s->y - TITLE_BAR_H - BORDER_W + (TITLE_BAR_H - MAX_BTN_SIZE) / 2;
    return (mx >= xb_x && mx < xb_x + MAX_BTN_SIZE && my >= xb_y && my < xb_y + MAX_BTN_SIZE);
}

static int hit_test_resize(struct xnx_surface *s, int mx, int my) {
    int rx = s->x - RESIZE_HANDLE;
    int ry = s->y - RESIZE_HANDLE;
    int rw = (int)s->width + 2*RESIZE_HANDLE;
    int rh = (int)s->height + 2*RESIZE_HANDLE;
    if (mx < rx || mx >= rx + rw || my < ry || my >= ry + rh) return 0;
    if (my < s->y && mx >= s->x - BORDER_W && mx < s->x + (int)s->width + BORDER_W) return 0;
    int edge = 0;
    if (mx < s->x) edge |= 1;
    if (mx >= s->x + (int)s->width) edge |= 2;
    if (my < s->y) edge |= 4;
    if (my >= s->y + (int)s->height) edge |= 8;
    return edge;
}

/* ─── surface compositing with decorations ──────────────────────────── */
static void composite_surfaces(void) {
    int z = 0;
    while (1) {
        struct xnx_surface *next = NULL;
        for (int i = 0; i < num_surfaces; ++i) {
            struct xnx_surface *s = &surfaces[i];
            if (!s->visible || !s->image || s->z_order < z || s->minimized) continue;
            if (!next || s->z_order < next->z_order) next = s;
        }
        if (!next) break;

        int win_x = next->x - BORDER_W;
        int win_y = next->y - TITLE_BAR_H - BORDER_W;
        int win_w = (int)next->width + 2 * BORDER_W;
        int win_h = (int)next->height + TITLE_BAR_H + 2 * BORDER_W;

        /* shadow */
        draw_shadow(next->x, next->y - TITLE_BAR_H, (int)next->width + 2*BORDER_W,
                    (int)next->height + TITLE_BAR_H + 2*BORDER_W, 8);

        /* border */
        uint32_t brd = next->focused ? COL_BORDER_FOC : COL_BORDER;
        draw_rect_outline(win_x, win_y, win_w, win_h, brd, BORDER_W);

        /* title bar */
        draw_rect(win_x + BORDER_W, win_y, win_w - 2*BORDER_W, TITLE_BAR_H, COL_TITLE_BG);

        /* title text */
        draw_text(win_x + BORDER_W + 8, win_y + (TITLE_BAR_H - font_height) / 2,
                  next->title, next->focused ? COL_TITLE_FG : COL_TEXT_DIM);

        /* close button */
        int cb_x = win_x + win_w - BORDER_W - CLOSE_BTN_SIZE - 6;
        int cb_y = win_y + (TITLE_BAR_H - CLOSE_BTN_SIZE) / 2;
        draw_rounded_rect(cb_x, cb_y, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE, 3,
                          (pointer_x >= cb_x && pointer_x < cb_x + CLOSE_BTN_SIZE &&
                           pointer_y >= cb_y && pointer_y < cb_y + CLOSE_BTN_SIZE) ?
                          COL_CLOSE_HOVER : COL_CLOSE_BTN);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_set_line_width(cr, 1.5);
        int m = 3;
        cairo_move_to(cr, (double)(cb_x + m), (double)(cb_y + m));
        cairo_line_to(cr, (double)(cb_x + CLOSE_BTN_SIZE - m), (double)(cb_y + CLOSE_BTN_SIZE - m));
        cairo_move_to(cr, (double)(cb_x + CLOSE_BTN_SIZE - m), (double)(cb_y + m));
        cairo_line_to(cr, (double)(cb_x + m), (double)(cb_y + CLOSE_BTN_SIZE - m));
        cairo_stroke(cr);

        /* minimize button */
        int mb_x = cb_x - MIN_BTN_SIZE - 4;
        int mb_y = win_y + (TITLE_BAR_H - MIN_BTN_SIZE) / 2;
        draw_rounded_rect(mb_x, mb_y, MIN_BTN_SIZE, MIN_BTN_SIZE, 3,
                          (pointer_x >= mb_x && pointer_x < mb_x + MIN_BTN_SIZE &&
                           pointer_y >= mb_y && pointer_y < mb_y + MIN_BTN_SIZE) ?
                          COL_MIN_BTN : 0x803FB950);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, (double)(mb_x + 3), (double)(mb_y + MIN_BTN_SIZE/2));
        cairo_line_to(cr, (double)(mb_x + MIN_BTN_SIZE - 3), (double)(mb_y + MIN_BTN_SIZE/2));
        cairo_stroke(cr);

        /* maximize button */
        int xb_x = mb_x - MAX_BTN_SIZE - 4;
        int xb_y = win_y + (TITLE_BAR_H - MAX_BTN_SIZE) / 2;
        draw_rounded_rect(xb_x, xb_y, MAX_BTN_SIZE, MAX_BTN_SIZE, 3,
                          (pointer_x >= xb_x && pointer_x < xb_x + MAX_BTN_SIZE &&
                           pointer_y >= xb_y && pointer_y < xb_y + MAX_BTN_SIZE) ?
                          COL_MAX_BTN : 0x8058A6FF);
        draw_rect_outline(xb_x + 3, xb_y + 3, MAX_BTN_SIZE - 6, MAX_BTN_SIZE - 6, COL_WHITE, 1.5);

        /* composite window content */
        int dx = next->x, dy = next->y;
        int sx = 0, sy = 0;
        int sw = (int)next->width, sh = (int)next->height;
        int cw = sw, ch = sh;
        if (dx < 0) { sx = -dx; cw += dx; dx = 0; }
        if (dy < 0) { sy = -dy; ch += dy; dy = 0; }
        if (dx < fb_width && dy < fb_height && sx < sw && sy < sh) {
            if (dx + cw > fb_width) cw = fb_width - dx;
            if (dy + ch > fb_height) ch = fb_height - dy;
            if (sx + cw > sw) cw = sw - sx;
            if (sy + ch > sh) ch = sh - sy;
            if (cw > 0 && ch > 0)
                pixman_image_composite32(PIXMAN_OP_OVER, next->image, NULL, framebuffer,
                    (unsigned)sx, (unsigned)sy, 0, 0, dx, dy, (unsigned)cw, (unsigned)ch);
        }
        z = next->z_order + 1;
    }
}

/* ─── protocol handlers ─────────────────────────────────────────────── */
static int handle_client_msg(struct xnx_client *cl) {
    uint8_t *p = cl->read_buf;
    uint32_t type = cl->current_header.type;
    uint32_t sz = cl->current_header.payload_size;

    switch (type) {
        case XNX_GET_DISPLAY_INFO: {
            struct xnx_display_info di = { .width = (uint32_t)fb_width, .height = (uint32_t)fb_height };
            return send_msg(cl->fd, XNX_DISPLAY_INFO, &di, sizeof(di));
        }
        case XNX_CREATE_SURFACE: {
            if (sz < sizeof(struct xnx_create_surface)) return -1;
            struct xnx_create_surface *r = (struct xnx_create_surface *)p;
            struct xnx_surface_created sc;
            if (create_surf(cl, r->width, r->height, &sc.surface_id) < 0) return -1;
            return send_msg(cl->fd, XNX_SURFACE_CREATED, &sc, sizeof(sc));
        }
        case XNX_DESTROY_SURFACE: {
            if (sz < 4) return -1;
            uint32_t sid; memcpy(&sid, p, 4);
            struct xnx_surface *s = find_surf(sid);
            if (!s || s->client_fd != cl->fd) return -1;
            drop_surf(sid);
            for (int i = 0; i < cl->num_surfaces; ++i) {
                if (cl->surface_ids[i] == sid) { cl->surface_ids[i] = cl->surface_ids[--cl->num_surfaces]; break; }
            }
            return 0;
        }
        case XNX_SET_TITLE: {
            if (sz < 5) return -1;
            uint32_t sid = *(uint32_t *)p;
            struct xnx_surface *s = find_surf(sid);
            if (!s || s->client_fd != cl->fd) return -1;
            strncpy(s->title, (const char *)(p + 4), 63);
            s->title[63] = '\0';
            return 0;
        }
        case XNX_SET_GEOMETRY: {
            if (sz < sizeof(struct xnx_set_geometry)) return -1;
            struct xnx_set_geometry *g = (struct xnx_set_geometry *)p;
            struct xnx_surface *s = find_surf(g->surface_id);
            if (!s || s->client_fd != cl->fd) return -1;
            s->x = g->x; s->y = g->y;
            if (g->width > 0 && g->height > 0 && (g->width != s->width || g->height != s->height)) {
                pixman_image_t *ni = pixman_image_create_bits(PIXMAN_a8r8g8b8, g->width, g->height, NULL, 0);
                if (ni) {
                    uint32_t cw = s->width < g->width ? s->width : g->width;
                    uint32_t ch = s->height < g->height ? s->height : g->height;
                    pixman_image_composite32(PIXMAN_OP_SRC, s->image, NULL, ni, 0, 0, 0, 0, 0, 0, cw, ch);
                    pixman_image_unref(s->image); s->image = ni;
                    s->width = g->width; s->height = g->height;
                }
            }
            return 0;
        }
        case XNX_WRITE_BUFFER: {
            if (sz < sizeof(struct xnx_write_buffer)) return -1;
            struct xnx_write_buffer *wb = (struct xnx_write_buffer *)p;
            struct xnx_surface *s = find_surf(wb->surface_id);
            if (!s || s->client_fd != cl->fd || !s->image) return -1;
            uint32_t w = wb->width, h = wb->height;
            if (w == 0 || h == 0) return 0;
            if (wb->x >= s->width || wb->y >= s->height) return 0;
            if (w > s->width - wb->x) w = s->width - wb->x;
            if (h > s->height - wb->y) h = s->height - wb->y;
            uint32_t *dst = pixman_image_get_data(s->image);
            uint32_t stride = (uint32_t)pixman_image_get_stride(s->image) / 4u;
            if (!dst || !stride) return 0;
            for (uint32_t row = 0; row < h; ++row) {
                size_t doff = ((size_t)(wb->y + row) * (size_t)stride + (size_t)wb->x);
                size_t soff = (size_t)row * (size_t)wb->width;
                memcpy(&dst[doff], p + sizeof(*wb) + soff * 4, (size_t)w * 4);
            }
            return 0;
        }
        case XNX_SET_CURSOR: {
            if (sz < sizeof(struct xnx_set_cursor)) return -1;
            struct xnx_set_cursor *sc = (struct xnx_set_cursor *)p;
            pointer_x = sc->x; pointer_y = sc->y;
            return 0;
        }
        case XNX_COMMIT: {
            if (sz < 4) return -1;
            uint32_t sid; memcpy(&sid, p, 4);
            struct xnx_surface *s = find_surf(sid);
            if (!s || s->client_fd != cl->fd) return -1;
            focus_surf(s);
            return send_msg(cl->fd, XNX_FRAME_DONE, NULL, 0);
        }
        case XNX_LIST_SURFACES: {
            uint32_t cnt = 0;
            for (int i = 0; i < num_surfaces; ++i) if (surfaces[i].visible) cnt++;
            struct xnx_header rh = { .type = XNX_SURFACE_LIST, .payload_size = 4 + cnt * sizeof(struct xnx_surface_info) };
            if (send_all(cl->fd, &rh, sizeof(rh)) < 0) return -1;
            uint32_t count_le = cnt;
            if (send_all(cl->fd, &count_le, 4) < 0) return -1;
            for (int i = 0; i < num_surfaces; ++i) {
                if (!surfaces[i].visible) continue;
                struct xnx_surface_info si;
                memset(&si, 0, sizeof(si));
                si.surface_id = surfaces[i].id; si.x = surfaces[i].x; si.y = surfaces[i].y;
                si.width = surfaces[i].width; si.height = surfaces[i].height;
                si.z_order = (uint32_t)surfaces[i].z_order;
                strncpy(si.title, surfaces[i].title, 63); si.title[63] = '\0';
                if (send_all(cl->fd, &si, sizeof(si)) < 0) return -1;
            }
            return 0;
        }
        case XNX_RAISE_SURFACE: {
            if (sz < 4) return -1;
            uint32_t sid; memcpy(&sid, p, 4);
            struct xnx_surface *s = find_surf(sid);
            if (!s) return -1;
            focus_surf(s);
            return 0;
        }
        case XNX_SHOW_CURSOR: {
            if (sz < sizeof(struct xnx_show_cursor)) return -1;
            struct xnx_show_cursor *sc = (struct xnx_show_cursor *)p;
            cursor_visible = sc->visible ? 1 : 0;
            return 0;
        }
        default: return -1;
    }
}

static int read_client(struct xnx_client *cl) {
    ssize_t n;
    if (cl->msg_state == 0) {
        int need = (int)sizeof(struct xnx_header) - cl->read_offset;
        n = recv(cl->fd, (uint8_t *)&cl->current_header + cl->read_offset, (size_t)need, 0);
        if (n <= 0) return (n == 0 || (errno != EINTR && errno != EAGAIN)) ? -1 : 0;
        cl->read_offset += (int)n;
        if (cl->read_offset == (int)sizeof(struct xnx_header)) {
            cl->read_offset = 0;
            if (cl->current_header.payload_size > sizeof(cl->read_buf)) return -1;
            if (cl->current_header.payload_size == 0) return handle_client_msg(cl);
            cl->msg_state = 1;
        }
        return 0;
    }
    n = recv(cl->fd, cl->read_buf + cl->read_offset, cl->current_header.payload_size - (uint32_t)cl->read_offset, 0);
    if (n <= 0) return (n == 0 || (errno != EINTR && errno != EAGAIN)) ? -1 : 0;
    cl->read_offset += (int)n;
    if (cl->read_offset == (int)cl->current_header.payload_size) {
        cl->read_offset = 0; cl->msg_state = 0;
        return handle_client_msg(cl);
    }
    return 0;
}

static int setup_socket(void) {
    unlink(XNX_SOCKET_PATH);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, XNX_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, XNX_BACKLOG) < 0) { close(fd); return -1; }
    return fd;
}

/* ─── main frame ────────────────────────────────────────────────────── */
static void draw_frame(void) {
    /* clear to desktop background */
    cairo_set_source_rgba(cr, 0.051, 0.067, 0.090, 1.0);
    cairo_paint(cr);

    /* composite client surfaces with decorations */
    composite_surfaces();

    /* draw cursor on top of everything */
    draw_cursor();

    /* flush cairo to pixel buffer */
    cairo_surface_flush(crs);

    /* copy to hardware framebuffer */
    flush_fb();
}

/* ─── main ──────────────────────────────────────────────────────────── */
int main(void) {
    struct xnx_fb_var_screeninfo vinfo;

    printf("XNX Compositor v1.0 (window manager) starting...\n");
    fflush(stdout);

    listener_fd = setup_socket();
    if (listener_fd < 0) return 1;

    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { fprintf(stderr, "Failed to open /dev/fb0\n"); return 1; }
    if (ioctl(fb_fd, XNX_FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "Failed fb info\n"); close(fb_fd); return 1;
    }
    /* release the framebuffer from the kernel console */
    ioctl(fb_fd, XNX_FBIODISOWN, 0);

    fb_width = (int)vinfo.xres; fb_height = (int)vinfo.yres;
    fb_bpp = (int)vinfo.bits_per_pixel; if (fb_bpp < 1) fb_bpp = 32;
    fb_hw_pitch = (size_t)vinfo.pitch;
    if (fb_hw_pitch == 0) fb_hw_pitch = (size_t)vinfo.xres_virtual * (size_t)vinfo.bits_per_pixel / 8u;
    if (fb_hw_pitch == 0) fb_hw_pitch = (size_t)fb_width * (size_t)(fb_bpp / 8);

    fb_size = (size_t)fb_width * (size_t)fb_height * 4u;
    de_pixels = calloc(1, fb_size);
    if (!de_pixels) { fprintf(stderr, "alloc fail\n"); close(fb_fd); return 1; }

    size_t hw_map_size = fb_hw_pitch * (size_t)fb_height;
    fb_hw_pixels = mmap(NULL, hw_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_hw_pixels == MAP_FAILED) { fprintf(stderr, "mmap fail\n"); free(de_pixels); close(fb_fd); return 1; }

    framebuffer = pixman_image_create_bits(PIXMAN_a8r8g8b8, fb_width, fb_height, de_pixels, fb_width * 4);
    if (!framebuffer) {
        fprintf(stderr, "pixman fail\n");
        munmap(fb_hw_pixels, hw_map_size); free(de_pixels); close(fb_fd); return 1;
    }

    mouse_fd = open("/dev/mouse", O_RDONLY);
    if (mouse_fd >= 0) {
        printf("WM: mouse opened\n");
        pointer_x = fb_width / 2;
        pointer_y = fb_height / 2;
    }

    /* init cairo */
    cairo_init();

    printf("WM listening on %s (%dx%d)\n", XNX_SOCKET_PATH, fb_width, fb_height);

    /* render first frame */
    draw_frame();

    while (1) {
        struct pollfd fds[MAX_CLIENTS + 3];
        int client_slots[MAX_CLIENTS], nfds = 0, mouse_idx = -1, tracked = 0;

        fds[nfds].fd = listener_fd; fds[nfds].events = POLLIN; fds[nfds].revents = 0; nfds++;
        if (mouse_fd >= 0) {
            mouse_idx = nfds;
            fds[nfds].fd = mouse_fd; fds[nfds].events = POLLIN; fds[nfds].revents = 0; nfds++;
        }
        for (int i = 0; i < num_clients; ++i) {
            if (!clients[i].active) continue;
            client_slots[tracked++] = i;
            fds[nfds].fd = clients[i].fd; fds[nfds].events = POLLIN; fds[nfds].revents = 0; nfds++;
        }

        if (poll(fds, (nfds_t)nfds, FRAME_INTERVAL_MS) < 0 && errno != EINTR) break;

        /* accept new connections */
        if (fds[0].revents & POLLIN) {
            struct sockaddr_un ca; socklen_t clen = sizeof(ca);
            int cf = accept(listener_fd, (struct sockaddr *)&ca, &clen);
            if (cf >= 0 && !add_client(cf)) close(cf);
        }

        /* mouse */
        if (mouse_idx >= 0 && (fds[mouse_idx].revents & POLLIN)) {
            struct { uint8_t buttons; int8_t dx; int8_t dy; } mp;
            if (read(mouse_fd, &mp, sizeof(mp)) == sizeof(mp)) {
                pointer_x += mp.dx; pointer_y -= mp.dy;
                pointer_x = clamp(pointer_x, 0, fb_width - 1);
                pointer_y = clamp(pointer_y, 0, fb_height - 1);

                int pressed = (mp.buttons & 1) && !(prev_buttons & 1);
                int released = !(mp.buttons & 1) && (prev_buttons & 1);
                int rpressed = (mp.buttons & 2) && !(prev_buttons & 2);
                prev_buttons = mp.buttons;

                /* dragging window */
                if (drag_surface_id >= 0 && (mp.buttons & 1)) {
                    struct xnx_surface *ds = find_surf((uint32_t)drag_surface_id);
                    if (ds) {
                        ds->x = pointer_x - drag_offset_x;
                        ds->y = pointer_y - drag_offset_y;
                    }
                }
                if (drag_surface_id >= 0 && released) drag_surface_id = -1;

                /* resizing window */
                if (resize_surface_id > 0 && (mp.buttons & 1)) {
                    struct xnx_surface *s = find_surf((uint32_t)resize_surface_id);
                    if (s) {
                        int dx = pointer_x - resize_orig_x - drag_offset_x;
                        int dy = pointer_y - resize_orig_y - drag_offset_y;
                        if (resize_edge & 1) {
                            int nw = resize_orig_w - dx;
                            if (nw > 100) { s->x = resize_orig_x + dx; s->width = (uint32_t)nw; }
                        }
                        if (resize_edge & 2) {
                            int nw = resize_orig_w + dx;
                            if (nw > 100) s->width = (uint32_t)nw;
                        }
                        if (resize_edge & 4) {
                            int nh = resize_orig_h - dy;
                            if (nh > 50) { s->y = resize_orig_y + dy; s->height = (uint32_t)nh; }
                        }
                        if (resize_edge & 8) {
                            int nh = resize_orig_h + dy;
                            if (nh > 50) s->height = (uint32_t)nh;
                        }
                    }
                }
                if (resize_surface_id > 0 && released) {
                    resize_surface_id = 0;
                    resize_edge = 0;
                }

                /* left click */
                if (pressed) {
                    int hit = 0;
                    for (int i = num_surfaces - 1; i >= 0; --i) {
                        struct xnx_surface *s = &surfaces[i];
                        if (!s->visible || !s->image || s->minimized) continue;

                        /* check close button */
                        if (hit_test_close(s, pointer_x, pointer_y)) {
                            send_msg(s->client_fd, XNX_CLOSE_REQUEST, NULL, 0);
                            drop_surf(s->id);
                            hit = 1; break;
                        }
                        /* check minimize button */
                        if (hit_test_min(s, pointer_x, pointer_y)) {
                            s->minimized = 1;
                            ensure_focus();
                            hit = 1; break;
                        }
                        /* check maximize button */
                        if (hit_test_max(s, pointer_x, pointer_y)) {
                            if (s->maximized) {
                                s->x = s->prev_x; s->y = s->prev_y;
                                s->width = (uint32_t)s->prev_w; s->height = (uint32_t)s->prev_h;
                                s->maximized = 0;
                            } else {
                                s->prev_x = s->x; s->prev_y = s->y;
                                s->prev_w = (int)s->width; s->prev_h = (int)s->height;
                                s->x = 0; s->y = 0;
                                s->width = (uint32_t)fb_width;
                                s->height = (uint32_t)fb_height;
                                s->maximized = 1;
                            }
                            hit = 1; break;
                        }
                        /* check resize handle */
                        int re = hit_test_resize(s, pointer_x, pointer_y);
                        if (re) {
                            resize_surface_id = (int)s->id;
                            resize_edge = re;
                            resize_orig_x = s->x; resize_orig_y = s->y;
                            resize_orig_w = (int)s->width; resize_orig_h = (int)s->height;
                            drag_offset_x = pointer_x - s->x;
                            drag_offset_y = pointer_y - s->y;
                            hit = 1; break;
                        }
                        /* check title bar (drag) */
                        if (hit_test_titlebar(s, pointer_x, pointer_y)) {
                            drag_surface_id = (int)s->id;
                            drag_offset_x = pointer_x - s->x;
                            drag_offset_y = pointer_y - s->y;
                            focus_surf(s);
                            hit = 1; break;
                        }
                        /* check window body */
                        int full_x = s->x - BORDER_W;
                        int full_y = s->y - TITLE_BAR_H - BORDER_W;
                        int full_w = (int)s->width + 2*BORDER_W;
                        int full_h = (int)s->height + TITLE_BAR_H + 2*BORDER_W;
                        if (pointer_x >= full_x && pointer_x < full_x + full_w &&
                            pointer_y >= full_y && pointer_y < full_y + full_h) {
                            focus_surf(s);
                            hit = 1; break;
                        }
                    }
                    if (!hit) {
                        focused_surface_id = 0;
                        ensure_focus();
                    }
                }

                /* right click: restore minimized window */
                if (rpressed) {
                    for (int i = num_surfaces - 1; i >= 0; --i) {
                        struct xnx_surface *s = &surfaces[i];
                        if (!s->visible || !s->image || !s->minimized) continue;
                        s->minimized = 0;
                        focus_surf(s);
                        break;
                    }
                }

                /* forward to focused surface */
                if (!drag_surface_id && !resize_surface_id) {
                    ensure_focus();
                    struct xnx_surface *s = find_surf(focused_surface_id);
                    if (s) {
                        struct xnx_pointer_event ev = {
                            .surface_id = s->id,
                            .x = pointer_x - s->x,
                            .y = pointer_y - s->y,
                            .buttons = mp.buttons,
                        };
                        send_msg(s->client_fd, XNX_POINTER_EVENT, &ev, sizeof(ev));
                    }
                }
            }
        }

        /* client messages */
        for (int i = 0, fi = 1 + (mouse_idx >= 0 ? 1 : 0); i < tracked; ++i, ++fi) {
            int ci = client_slots[i];
            if (ci >= num_clients || !clients[ci].active) continue;
            if (fds[fi].revents & (POLLIN | POLLHUP | POLLERR))
                if (read_client(&clients[ci]) < 0) remove_client(ci);
        }

        /* render frame */
        draw_frame();
    }

    if (listener_fd >= 0) close(listener_fd);
    if (mouse_fd >= 0) close(mouse_fd);
    if (cr) cairo_destroy(cr);
    if (crs) cairo_surface_destroy(crs);
    if (framebuffer) pixman_image_unref(framebuffer);
    if (fb_hw_pixels && fb_hw_pixels != MAP_FAILED) munmap(fb_hw_pixels, fb_hw_pitch * (size_t)fb_height);
    if (de_pixels) free(de_pixels);
    close(fb_fd); unlink(XNX_SOCKET_PATH);
    return 0;
}
