#include <errno.h>
#include <fcntl.h>
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

#include "protocol.h"

#define MAX_SURFACES_PER_CLIENT 16
#define MAX_CLIENTS 32
#define MAX_SURFACES 256
#define READ_BUF_SIZE XNX_MAX_MESSAGE_BYTES
#define FRAME_INTERVAL_MS 16

struct xnx_surface {
    uint32_t id;
    int client_fd;
    pixman_image_t *image;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    char title[64];
    int z_order;
    int visible;
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

static struct xnx_surface surfaces[MAX_SURFACES];
static int num_surfaces;
static uint32_t next_surface_id;
static int next_z_order = 1;

static struct xnx_client clients[MAX_CLIENTS];
static int num_clients;
static uint32_t next_client_id;

static int listener_fd = -1;
static pixman_image_t *framebuffer;
static int fb_width;
static int fb_height;
static size_t fb_size;
static uint32_t *fb_pixels;
static uint32_t *fb_hw_pixels;
static size_t fb_hw_pitch;
static int fb_bpp;
static uint32_t focused_surface_id;

static int mouse_fd = -1;
static int pointer_x;
static int pointer_y;
static uint8_t cursor_type = 1;
static int cursor_visible;

static void draw_cursor(void) {
    if (!cursor_visible || !fb_pixels || mouse_fd < 0) return;
    int cx = pointer_x, cy = pointer_y;
    uint32_t color = 0xFFFFFFFF, outline = 0x00000000;
    static const uint8_t arrow[16] = {0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF,0xFF,0xF0,0xB0,0x18,0x18,0x0C,0x0C,0x06};
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (!(arrow[row] & (0x80 >> col))) continue;
            int px = cx + col, py = cy + row;
            if (px < 0 || px >= fb_width || py < 0 || py >= fb_height) continue;
            size_t off = (size_t)py * (size_t)fb_width + (size_t)px;
            if (col == 0 || row == 0 || !(arrow[row] & (0x80 >> (col - 1))) || !(arrow[row - 1] & (0x80 >> col)))
                fb_pixels[off] = outline;
            else fb_pixels[off] = color;
        }
    }
}

static int send_all(int fd, const void *buffer, size_t length) {
    const uint8_t *c = (const uint8_t *)buffer;
    size_t rem = length;
    while (rem > 0) {
        ssize_t s = send(fd, c, rem, 0);
        if (s > 0) { c += (size_t)s; rem -= (size_t)s; continue; }
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

static struct xnx_surface *find_surf(uint32_t id) {
    for (int i = 0; i < num_surfaces; ++i) if (surfaces[i].id == id) return &surfaces[i];
    return NULL;
}

static int surf_idx(uint32_t id) {
    for (int i = 0; i < num_surfaces; ++i) if (surfaces[i].id == id) return i;
    return -1;
}

static void ensure_focus(void) {
    struct xnx_surface *f = find_surf(focused_surface_id);
    if (f && f->visible && f->image) return;
    struct xnx_surface *best = NULL;
    for (int i = 0; i < num_surfaces; ++i) {
        if (!surfaces[i].visible || !surfaces[i].image) continue;
        if (!best || surfaces[i].z_order > best->z_order) best = &surfaces[i];
    }
    focused_surface_id = best ? best->id : 0;
}

static void focus_surf(struct xnx_surface *s) {
    if (!s) { ensure_focus(); return; }
    s->z_order = next_z_order++;
    focused_surface_id = s->id;
}

static void flush_fb(void) {
    if (!fb_hw_pixels || !fb_pixels || !fb_hw_pitch) return;
    for (int y = 0; y < fb_height; ++y) {
        uint8_t *hw = (uint8_t *)fb_hw_pixels + (size_t)y * fb_hw_pitch;
        uint8_t *sw = (uint8_t *)fb_pixels + (size_t)y * (size_t)fb_width * 4u;
        for (int x = 0; x < fb_width; ++x) { hw[x*3] = sw[x*4]; hw[x*3+1] = sw[x*4+1]; hw[x*3+2] = sw[x*4+2]; }
    }
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
    if (num_surfaces == 0) memset(fb_pixels, 0, fb_size);
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
    s->x = (fb_width - (int)w) / 2; s->y = (fb_height - (int)h) / 2;
    cl->surface_ids[cl->num_surfaces++] = id;
    *out = id; focus_surf(s);
    return 0;
}

/* ---- Compositing ---- */

static void composite_surfaces(void) {
    if (!framebuffer || !fb_pixels) return;
    memset(fb_pixels, 0, fb_size);

    int z = 0;
    while (1) {
        struct xnx_surface *next = NULL;
        for (int i = 0; i < num_surfaces; ++i) {
            struct xnx_surface *s = &surfaces[i];
            if (!s->visible || !s->image || s->z_order < z) continue;
            if (!next || s->z_order < next->z_order) next = s;
        }
        if (!next) break;

        int dx = next->x, dy = next->y, sx = 0, sy = 0;
        unsigned int sw = next->width, sh = next->height;
        int w = (int)sw, h = (int)sh;

        if (dx < 0) { sx = -dx; w += dx; dx = 0; }
        if (dy < 0) { sy = -dy; h += dy; dy = 0; }
        if (dx >= fb_width || dy >= fb_height || sx >= (int)sw || sy >= (int)sh) { z = next->z_order + 1; continue; }
        if (dx + w > fb_width) w = fb_width - dx;
        if (dy + h > fb_height) h = fb_height - dy;
        if (sx + w > (int)sw) w = (int)sw - sx;
        if (sy + h > (int)sh) h = (int)sh - sy;

        if (w > 0 && h > 0)
            pixman_image_composite32(PIXMAN_OP_OVER, next->image, NULL, framebuffer,
                sx, sy, 0, 0, dx, dy, (unsigned)w, (unsigned)h);
        z = next->z_order + 1;
    }
    flush_fb();
}

/* ---- Protocol handlers ---- */

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
            pointer_x = sc->x; pointer_y = sc->y; cursor_type = sc->cursor_type;
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
    if (fd < 0) { fprintf(stderr, "socket failed\n"); return -1; }
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, XNX_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { fprintf(stderr, "bind failed\n"); close(fd); return -1; }
    if (listen(fd, XNX_BACKLOG) < 0) { fprintf(stderr, "listen failed\n"); close(fd); return -1; }
    int fl = fcntl(fd, F_GETFD, 0); if (fl >= 0) fcntl(fd, F_SETFD, fl | FD_CLOEXEC);
    return fd;
}

int main(void) {
    int fb_fd;
    struct xnx_fb_var_screeninfo vinfo;

    printf("XNX Compositor v0.4 (display server) starting...\n");
    fflush(stdout);

    listener_fd = setup_socket();
    if (listener_fd < 0) return 1;

    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { fprintf(stderr, "Failed to open /dev/fb0\n"); return 1; }
    if (ioctl(fb_fd, XNX_FBIOGET_VSCREENINFO, &vinfo) < 0) { fprintf(stderr, "Failed fb info\n"); close(fb_fd); return 1; }

    fb_width = (int)vinfo.xres; fb_height = (int)vinfo.yres;
    fb_bpp = (int)vinfo.bits_per_pixel; if (fb_bpp < 1) fb_bpp = 32;
    fb_hw_pitch = (size_t)vinfo.xres_virtual * (size_t)vinfo.bits_per_pixel / 8u;
    if (fb_hw_pitch == 0) fb_hw_pitch = (size_t)fb_width * 4u;

    fb_size = (size_t)fb_width * (size_t)fb_height * 4u;
    fb_pixels = calloc(1, fb_size);
    if (!fb_pixels) { fprintf(stderr, "alloc fail\n"); close(fb_fd); return 1; }

    size_t hw_map_size = fb_hw_pitch * (size_t)fb_height;
    fb_hw_pixels = mmap(NULL, hw_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_hw_pixels == MAP_FAILED) { fprintf(stderr, "mmap fail\n"); free(fb_pixels); close(fb_fd); return 1; }

    framebuffer = pixman_image_create_bits(PIXMAN_a8r8g8b8, fb_width, fb_height, fb_pixels, fb_width * 4);
    if (!framebuffer) { fprintf(stderr, "pixman fail\n"); munmap(fb_hw_pixels, hw_map_size); free(fb_pixels); close(fb_fd); return 1; }

    memset(fb_pixels, 0, fb_size); flush_fb();

    mouse_fd = open("/dev/mouse", O_RDONLY);
    if (mouse_fd >= 0) { printf("XNX: mouse opened\n"); pointer_x = fb_width / 2; pointer_y = fb_height / 2; }
    else printf("XNX: no mouse\n");

    printf("Listening on %s (%dx%d)\n", XNX_SOCKET_PATH, fb_width, fb_height);

    while (1) {
        struct pollfd fds[MAX_CLIENTS + 3];
        int client_slots[MAX_CLIENTS], nfds = 0, stdin_idx = -1, mouse_idx = -1, tracked = 0;

        fds[nfds].fd = listener_fd; fds[nfds].events = POLLIN; fds[nfds].revents = 0; nfds++;

        if (num_surfaces > 0) { stdin_idx = nfds; fds[nfds].fd = STDIN_FILENO; fds[nfds].events = POLLIN; fds[nfds].revents = 0; nfds++; }
        if (mouse_fd >= 0) { mouse_idx = nfds; fds[nfds].fd = mouse_fd; fds[nfds].events = POLLIN; fds[nfds].revents = 0; nfds++; }

        for (int i = 0; i < num_clients; ++i) {
            if (!clients[i].active) continue;
            client_slots[tracked++] = i;
            fds[nfds].fd = clients[i].fd; fds[nfds].events = POLLIN; fds[nfds].revents = 0; nfds++;
        }

        if (poll(fds, (nfds_t)nfds, FRAME_INTERVAL_MS) < 0 && errno != EINTR) break;

        if (fds[0].revents & POLLIN) {
            struct sockaddr_un ca; socklen_t clen = sizeof(ca);
            int cf = accept(listener_fd, (struct sockaddr *)&ca, &clen);
            if (cf >= 0 && !add_client(cf)) close(cf);
        }

        if (stdin_idx >= 0 && (fds[stdin_idx].revents & POLLIN)) {
            unsigned char ch;
            if (read(STDIN_FILENO, &ch, 1) == 1) {
                ensure_focus();
                struct xnx_surface *s = find_surf(focused_surface_id);
                if (s) {
                    struct xnx_key_event ke = { .surface_id = s->id, .keycode = ch, .pressed = 1 };
                    send_msg(s->client_fd, XNX_KEY_EVENT, &ke, sizeof(ke));
                }
            }
        }

        if (mouse_idx >= 0 && (fds[mouse_idx].revents & POLLIN)) {
            struct { uint8_t buttons; int8_t dx; int8_t dy; } mp;
            if (read(mouse_fd, &mp, sizeof(mp)) == sizeof(mp)) {
                pointer_x += mp.dx; pointer_y -= mp.dy;
                if (pointer_x < 0) pointer_x = 0; if (pointer_y < 0) pointer_y = 0;
                if (pointer_x >= fb_width) pointer_x = fb_width - 1;
                if (pointer_y >= fb_height) pointer_y = fb_height - 1;

                /* Click-to-focus: on button press, find surface under cursor and focus it */
                static uint8_t prev_buttons = 0;
                int pressed = (mp.buttons & 1) && !(prev_buttons & 1);
                prev_buttons = mp.buttons;

                if (pressed) {
                    for (int i = num_surfaces - 1; i >= 0; --i) {
                        if (!surfaces[i].visible || !surfaces[i].image) continue;
                        int sx = surfaces[i].x, sy = surfaces[i].y;
                        int sw = (int)surfaces[i].width, sh = (int)surfaces[i].height;
                        if (pointer_x >= sx && pointer_x < sx + sw && pointer_y >= sy && pointer_y < sy + sh) {
                            focus_surf(&surfaces[i]);
                            break;
                        }
                    }
                }

                /* Forward pointer event to focused surface */
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

        int num_extra = (stdin_idx >= 0 ? 1 : 0) + (mouse_idx >= 0 ? 1 : 0);
        for (int i = 0, fi = 1 + num_extra; i < tracked; ++i, ++fi) {
            int ci = client_slots[i];
            if (ci >= num_clients || !clients[ci].active) continue;
            if (fds[fi].revents & (POLLIN | POLLHUP | POLLERR))
                if (read_client(&clients[ci]) < 0) remove_client(ci);
        }

        composite_surfaces();
        if (mouse_fd >= 0 && fb_pixels) { draw_cursor(); flush_fb(); }
    }

    if (listener_fd >= 0) close(listener_fd);
    if (mouse_fd >= 0) close(mouse_fd);
    if (framebuffer) pixman_image_unref(framebuffer);
    if (fb_hw_pixels && fb_hw_pixels != MAP_FAILED) munmap(fb_hw_pixels, fb_hw_pitch * (size_t)fb_height);
    if (fb_pixels) free(fb_pixels);
    close(fb_fd); unlink(XNX_SOCKET_PATH);
    return 0;
}
