#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <ohui/ohui.h>
#include <xnx/xnx.h>
#include <xnx/protocol.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MENU_COUNT 4

static const char *menu_items[] = {
    "Install OpenHobbyOS",
    "About",
    "Exit to shell",
    ""
};

static int selected;
static FT_Library ft_lib;
static FT_Face ft_face;

static int load_font(void) {
    if (FT_Init_FreeType(&ft_lib)) return -1;
    if (FT_New_Face(ft_lib, "/fonts/Monospace.ttf", 0, &ft_face)) return -1;
    FT_Set_Pixel_Sizes(ft_face, 0, 14);
    cairo_font_face_t *cf = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
    ohui_set_font_face(cf, 14.0);
    return 0;
}

static void draw_installer(cairo_t *cr, int fb_w, int fb_h, int mx, int my) {
    for (int y = 0; y < fb_h; y++) {
        double t = (double)y / fb_h;
        double r = 0.08 + t * 0.12;
        double g = 0.08 + t * 0.10;
        double b = 0.20 + t * 0.15;
        cairo_set_source_rgb(cr, r, g, b);
        cairo_rectangle(cr, 0, y, fb_w, 1);
        cairo_fill(cr);
    }

    ohui_label_t title;
    title.rect.x = 40; title.rect.y = 30;
    title.rect.w = fb_w - 80; title.rect.h = 30;
    snprintf(title.text, sizeof(title.text), "OpenHobbyOS Installer");
    title.banner = 1;
    ohui_label_draw(cr, &title);

    ohui_label_t subtitle;
    subtitle.rect.x = 44; subtitle.rect.y = 62;
    subtitle.rect.w = fb_w - 88; subtitle.rect.h = 18;
    snprintf(subtitle.text, sizeof(subtitle.text), "Graphical OS Installation Wizard");
    subtitle.banner = 0;
    ohui_label_draw(cr, &subtitle);

    cairo_set_source_rgb(cr, 0.3, 0.3, 0.5);
    cairo_rectangle(cr, 40, 88, fb_w - 80, 1);
    cairo_fill(cr);

    for (int i = 0; menu_items[i][0]; i++) {
        int bx = 60;
        int by = 120 + i * 48;
        int bw = fb_w - 120;
        int bh = 36;

        ohui_button_t btn;
        btn.rect.x = bx; btn.rect.y = by;
        btn.rect.w = bw; btn.rect.h = bh;
        snprintf(btn.text, sizeof(btn.text), "%s", menu_items[i]);
        btn.state = (i == selected) ? OHUI_BTN_PRESSED : OHUI_BTN_NORMAL;
        btn.hover = (my >= bx && my < bx + bw && my >= by && my < by + bh);
        ohui_button_draw(cr, &btn);

        if (i == selected) {
            cairo_set_source_rgb(cr, 0.2, 0.3, 0.6);
            cairo_set_line_width(cr, 2);
            cairo_rectangle(cr, bx - 1, by - 1, bw + 2, bh + 2);
            cairo_stroke(cr);
        }
    }

    ohui_label_t hint;
    hint.rect.x = 40; hint.rect.y = fb_h - 30;
    hint.rect.w = fb_w - 80; hint.rect.h = 18;
    snprintf(hint.text, sizeof(hint.text), "UP/DOWN arrows: navigate   ENTER: select   ESC: exit");
    hint.banner = 0;
    ohui_label_draw(cr, &hint);
}

int main(void) {
    xnx_conn_t *conn;
    uint32_t fb_w, fb_h;
    uint32_t surf;
    uint32_t *pixels;
    cairo_surface_t *cairo_surf;
    cairo_t *cr;

    load_font();

    conn = xnx_connect(NULL);
    if (!conn) { fprintf(stderr, "installer: xnx connect failed\n"); return 1; }

    if (xnx_get_display_info(conn, &fb_w, &fb_h) < 0) { fprintf(stderr, "installer: get display info failed\n"); xnx_disconnect(conn); return 1; }
    if (xnx_create_surface(conn, fb_w, fb_h, &surf) < 0) { fprintf(stderr, "installer: create surface failed\n"); xnx_disconnect(conn); return 1; }

    xnx_set_title(conn, surf, "Installer");
    xnx_set_geometry(conn, surf, 0, 0, fb_w, fb_h);
    xnx_show_cursor(conn, 1);

    pixels = calloc((size_t)fb_w * (size_t)fb_h, 4);
    if (!pixels) { xnx_destroy_surface(conn, surf); xnx_disconnect(conn); return 1; }

    cairo_surf = cairo_image_surface_create_for_data(
        (unsigned char *)pixels, CAIRO_FORMAT_ARGB32, (int)fb_w, (int)fb_h, (int)fb_w * 4);
    cr = cairo_create(cairo_surf);

    selected = 0;
    int mx = 0, my = 0;
    int running = 1;

    while (running) {
        draw_installer(cr, fb_w, fb_h, mx, my);
        xnx_write_buffer(conn, surf, 0, 0, fb_w, fb_h, pixels);
        xnx_commit(conn, surf);

        int got_event = 0;
        while (!got_event) {
            struct xnx_event ev;
            int r = xnx_poll_event(conn, &ev);
            if (r > 0) {
                got_event = 1;
                if (ev.type == XNX_EVENT_POINTER) {
                    mx = ev.data.pointer.x;
                    my = ev.data.pointer.y;
                    if (ev.data.pointer.buttons & 1) {
                        for (int i = 0; menu_items[i][0]; i++) {
                            int bx = 60, by = 120 + i * 48, bw = fb_w - 120, bh = 36;
                            if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) {
                                selected = i;
                                if (selected == 0) { }
                                else if (selected == 1) { }
                                else if (selected == 2) running = 0;
                            }
                        }
                    }
                }
                if (ev.type == XNX_EVENT_KEY && ev.data.key.pressed) {
                    switch (ev.data.key.keycode) {
                        case 0x48: if (selected > 0) selected--; break;
                        case 0x50: if (menu_items[selected + 1][0]) selected++; break;
                        case 0x1C:
                            if (selected == 0) {
                                draw_installer(cr, fb_w, fb_h, mx, my);
                                cairo_set_source_rgb(cr, 0.6, 0.8, 0.6);
                                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                                cairo_set_font_size(cr, 14);
                                cairo_move_to(cr, 60, 320);
                                cairo_show_text(cr, "Installing... (not yet implemented)");
                                xnx_write_buffer(conn, surf, 0, 0, fb_w, fb_h, pixels);
                                xnx_commit(conn, surf);
                                usleep(2000000);
                            } else if (selected == 2) {
                                running = 0;
                            }
                            break;
                        case 0x01: running = 0; break;
                    }
                }
            } else if (r < 0) {
                running = 0;
                break;
            }
        }
    }

    xnx_show_cursor(conn, 0);
    cairo_destroy(cr);
    cairo_surface_destroy(cairo_surf);
    free(pixels);
    xnx_destroy_surface(conn, surf);
    xnx_disconnect(conn);
    return 0;
}
