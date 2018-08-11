#ifndef XNX_CLIENT_H
#define XNX_CLIENT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xnx_connection;
typedef struct xnx_connection xnx_conn_t;

/* Protocol types for surface info are in protocol.h */
/* For convenience, forward-declare what we need for the API */
struct xnx_surface_info;

enum xnx_event_type {
    XNX_EVENT_NONE = 0,
    XNX_EVENT_KEY = 1,
    XNX_EVENT_POINTER = 2,
};

struct xnx_event {
    uint32_t type;
    union {
        struct {
            uint32_t surface_id;
            uint32_t keycode;
            uint8_t pressed;
        } key;
        struct {
            uint32_t surface_id;
            int32_t x;
            int32_t y;
            uint8_t buttons;
            int8_t dx;
            int8_t dy;
        } pointer;
    } data;
};

xnx_conn_t *xnx_connect(const char *socket_path);
void xnx_disconnect(xnx_conn_t *c);

int xnx_get_display_info(xnx_conn_t *c, uint32_t *out_width, uint32_t *out_height);
int xnx_create_surface(xnx_conn_t *c, uint32_t width, uint32_t height, uint32_t *out_id);
int xnx_destroy_surface(xnx_conn_t *c, uint32_t surface_id);
int xnx_set_title(xnx_conn_t *c, uint32_t surface_id, const char *title);
int xnx_set_geometry(xnx_conn_t *c, uint32_t surface_id, int32_t x, int32_t y, uint32_t width, uint32_t height);
int xnx_write_buffer(xnx_conn_t *c, uint32_t surface_id, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const uint32_t *pixels);
int xnx_commit(xnx_conn_t *c, uint32_t surface_id);
int xnx_set_cursor(xnx_conn_t *c, int32_t x, int32_t y, uint8_t cursor_type);

/* WM / DE extensions */
int xnx_list_surfaces(xnx_conn_t *c, struct xnx_surface_info *surfaces, uint32_t *count);
int xnx_raise_surface(xnx_conn_t *c, uint32_t surface_id);
int xnx_show_cursor(xnx_conn_t *c, int visible);

int xnx_get_fd(xnx_conn_t *c);
int xnx_poll_event(xnx_conn_t *c, struct xnx_event *out_event);
int xnx_dispatch(xnx_conn_t *c);

#ifdef __cplusplus
}
#endif

#endif
