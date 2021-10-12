#include "panic.h"
#include "console.h"
#include "console_font.h"
#include "format.h"
#include "io.h"
#include "power.h"
#include "string.h"
#include "panic_logo.h"

static u32 panic_scale_channel(u8 value, u8 mask_size) {
    if (mask_size >= 8) return (u32)value << (mask_size - 8);
    if (mask_size == 0) return 0;
    return ((u32)value * ((1u << mask_size) - 1u) + 127u) / 255u;
}

static u32 panic_pack_colour(u8 r, u8 g, u8 b, const console_fb_info_t *fb) {
    return (panic_scale_channel(r, fb->red_mask_size) << fb->red_shift) |
           (panic_scale_channel(g, fb->green_mask_size) << fb->green_shift) |
           (panic_scale_channel(b, fb->blue_mask_size) << fb->blue_shift);
}

static void panic_fill_fb(const console_fb_info_t *fb, u8 r, u8 g, u8 b) {
    u32 color = panic_pack_colour(r, g, b, fb);
    u32 bytes_pp = (fb->bpp + 7u) / 8u;
    for (u32 y = 0; y < fb->height; y++) {
        volatile u8 *row = fb->address + y * fb->pitch;
        for (u32 x = 0; x < fb->width; x++) {
            u32 *pixel = (u32 *)(row + x * bytes_pp);
            *pixel = color;
        }
    }
}

static void panic_draw_image(const console_fb_info_t *fb,
                              u32 dst_x, u32 dst_y,
                              u32 img_w, u32 img_h,
                              const unsigned char *rgba) {
    u32 bytes_pp = (fb->bpp + 7u) / 8u;
    for (u32 y = 0; y < img_h && (dst_y + y) < fb->height; y++) {
        for (u32 x = 0; x < img_w && (dst_x + x) < fb->width; x++) {
            const u8 *p = rgba + (y * img_w + x) * 4;
            if (p[3] < 16) continue;
            volatile u8 *fbp = fb->address + (dst_y + y) * fb->pitch + (dst_x + x) * bytes_pp;
            if (p[3] >= 254) {
                *(u32 *)fbp = panic_pack_colour(p[0], p[1], p[2], fb);
            } else {
                u32 bg = *(u32 *)fbp;
                u32 er = (bg >> fb->red_shift) & ((1u << fb->red_mask_size) - 1u);
                u32 eg = (bg >> fb->green_shift) & ((1u << fb->green_mask_size) - 1u);
                u32 eb = (bg >> fb->blue_shift) & ((1u << fb->blue_mask_size) - 1u);
                if (fb->red_mask_size >= 8) {
                    er >>= (fb->red_mask_size - 8);
                } else {
                    er = (er * 255 + ((1u << fb->red_mask_size) - 1u) / 2) / ((1u << fb->red_mask_size) - 1u);
                }
                if (fb->green_mask_size >= 8) {
                    eg >>= (fb->green_mask_size - 8);
                } else {
                    eg = (eg * 255 + ((1u << fb->green_mask_size) - 1u) / 2) / ((1u << fb->green_mask_size) - 1u);
                }
                if (fb->blue_mask_size >= 8) {
                    eb >>= (fb->blue_mask_size - 8);
                } else {
                    eb = (eb * 255 + ((1u << fb->blue_mask_size) - 1u) / 2) / ((1u << fb->blue_mask_size) - 1u);
                }
                u32 a = p[3];
                u32 nr = ((u32)p[0] * a + er * (255 - a)) / 255;
                u32 ng = ((u32)p[1] * a + eg * (255 - a)) / 255;
                u32 nb = ((u32)p[2] * a + eb * (255 - a)) / 255;
                u32 nc = panic_pack_colour((u8)nr, (u8)ng, (u8)nb, fb);
                *(u32 *)fbp = nc;
            }
        }
    }
}

static void panic_draw_char(const console_fb_info_t *fb,
                             u32 x, u32 y, char ch,
                             u32 fg_color, u32 bg_color) {
    u32 bytes_pp = (fb->bpp + 7u) / 8u;
    unsigned int glyph = (unsigned char)ch;
    for (u32 row = 0; row < CONSOLE_FONT_HEIGHT && (y + row) < fb->height; row++) {
        unsigned char bits = console_font[glyph * CONSOLE_FONT_HEIGHT + row];
        for (u32 col = 0; col < CONSOLE_FONT_WIDTH && (x + col) < fb->width; col++) {
            u32 color = (bits & (0x80 >> col)) ? fg_color : bg_color;
            volatile u8 *pixel = fb->address + (y + row) * fb->pitch + (x + col) * bytes_pp;
            *(u32 *)pixel = color;
        }
    }
}

static void panic_draw_text(const console_fb_info_t *fb,
                             u32 x, u32 y, const char *text,
                             u32 fg_color, u32 bg_color) {
    u32 sx = x;
    while (*text) {
        if (*text == '\n') {
            x = sx;
            y += CONSOLE_FONT_HEIGHT + 4;
        } else {
            panic_draw_char(fb, x, y, *text, fg_color, bg_color);
            x += CONSOLE_FONT_WIDTH;
        }
        text++;
    }
}

static void panic_itoa(int num, char *buf) {
    char tmp[16];
    int i = 0, j = 0;
    if (num == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (num > 0 && i < 14) {
        tmp[i++] = "0123456789"[num % 10];
        num /= 10;
    }
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

NORETURN void panic_v(const char *fmt, va_list args) {
    console_fb_info_t fb;
    u32 fg_white, fg_red, fg_yellow, bg_black;
    char msg_buf[512];
    char time_buf[32];
    u32 img_x, img_y;
    u32 text_x, text_y;
    u32 row_y;
    int i;

    interrupts_disable();

    if (console_get_fb_info(&fb) == 0 || fb.address == NULL) {
        console_set_color(CONSOLE_WHITE, CONSOLE_RED);
        console_write("\nPANIC: ");
        console_vprintf(fmt, args);
        console_write("\nSystem halted.\n");
        for (;;) cpu_halt();
    }

    bg_black = panic_pack_colour(0x00, 0x00, 0x00, &fb);
    fg_white = panic_pack_colour(0xff, 0xff, 0xff, &fb);
    fg_red   = panic_pack_colour(0xff, 0x55, 0x55, &fb);
    fg_yellow = panic_pack_colour(0xff, 0xff, 0x55, &fb);

    panic_fill_fb(&fb, 0x33, 0x00, 0x00);

    img_x = (fb.width - PANIC_LOGO_WIDTH) / 2;
    img_y = 40;
    panic_draw_image(&fb, img_x, img_y, PANIC_LOGO_WIDTH, PANIC_LOGO_HEIGHT, PANIC_LOGO);

    text_x = 40;
    text_y = img_y + PANIC_LOGO_HEIGHT + 30;

    row_y = text_y;
    panic_draw_text(&fb, text_x, row_y, "  SYSTEM PANIC", fg_red, bg_black);
    row_y += CONSOLE_FONT_HEIGHT + 8;

    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    {
        char *line = msg_buf;
        char *nl;
        while (line && *line) {
            nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            panic_draw_text(&fb, text_x, row_y, line, fg_yellow, bg_black);
            row_y += CONSOLE_FONT_HEIGHT + 4;
            if (nl) {
                *nl = '\n';
                line = nl + 1;
            } else {
                break;
            }
        }
    }

    row_y += 20;

    panic_draw_text(&fb, text_x, row_y,
                     "The system encountered a fatal error and cannot continue.",
                     fg_white, bg_black);
    row_y += CONSOLE_FONT_HEIGHT + 4;
    panic_draw_text(&fb, text_x, row_y,
                     "The system will reboot automatically.",
                     fg_white, bg_black);
    row_y += CONSOLE_FONT_HEIGHT + 16;

    {
        u32 count_y = row_y;
        for (i = 20; i > 0; i--) {
            panic_itoa(i, time_buf);
            panic_draw_text(&fb, text_x, count_y,
                            "Rebooting in                    seconds.",
                            fg_red, bg_black);
            panic_draw_text(&fb, text_x + 13 * CONSOLE_FONT_WIDTH,
                            count_y, time_buf, fg_white, bg_black);
            for (volatile u32 d = 0; d < 2000000; d++) {
                cpu_pause();
            }
        }
    }

    power_reboot();
    for (;;) cpu_halt();
}

NORETURN void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    panic_v(fmt, args);
    va_end(args);
}
