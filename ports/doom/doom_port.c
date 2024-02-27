#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <xnx/xnx.h>
#include <xnx/protocol.h>

static xnx_conn_t *s_xnx = NULL;
static uint32_t s_surface_id = 0;
static uint32_t s_SurfaceWidth = DOOMGENERIC_RESX;
static uint32_t s_SurfaceHeight = DOOMGENERIC_RESY;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(uint32_t keycode) {
    switch (keycode) {
    case 0x1C: case 0x9C: return KEY_ENTER;
    case 0x01: return KEY_ESCAPE;
    case 0x4B: case 0xCB: return KEY_LEFTARROW;
    case 0x4D: case 0xCD: return KEY_RIGHTARROW;
    case 0x48: case 0xC8: return KEY_UPARROW;
    case 0x50: case 0xD0: return KEY_DOWNARROW;
    case 0x1D: return KEY_FIRE;
    case 0x39: return KEY_USE;
    case 0x2A: case 0x36: return KEY_RSHIFT;
    case 0x15: return 'y';
    case 0x31: return 'n';
    default: return 0;
    }
}

static void addKeyToQueue(int pressed, uint32_t keycode) {
    unsigned char key = convertToDoomKey(keycode);
    if (key == 0) return;
    unsigned short keyData = (pressed << 8) | key;
    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex = (s_KeyQueueWriteIndex + 1) % KEYQUEUE_SIZE;
}

void DG_Init() {
    s_xnx = xnx_connect(XNX_SOCKET_PATH);
    if (!s_xnx) {
        fprintf(stderr, "doom: failed to connect to compositor\n");
        exit(1);
    }

    uint32_t disp_w, disp_h;
    if (xnx_get_display_info(s_xnx, &disp_w, &disp_h) < 0) {
        fprintf(stderr, "doom: failed to get display info\n");
        exit(1);
    }

    if (xnx_create_surface(s_xnx, DOOMGENERIC_RESX, DOOMGENERIC_RESY, &s_surface_id) < 0) {
        fprintf(stderr, "doom: failed to create surface\n");
        exit(1);
    }

    xnx_set_title(s_xnx, s_surface_id, "DOOM");
    xnx_commit(s_xnx, s_surface_id);

    printf("doom: surface %u created, %dx%d (display %ux%u)\n",
           s_surface_id, DOOMGENERIC_RESX, DOOMGENERIC_RESY, disp_w, disp_h);
}

static void handleXnxEvents(void) {
    if (!s_xnx) return;

    struct xnx_event ev;
    while (xnx_poll_event(s_xnx, &ev) == 0) {
        switch (ev.type) {
        case XNX_EVENT_KEY:
            if (ev.data.key.surface_id == s_surface_id) {
                addKeyToQueue(ev.data.key.pressed, ev.data.key.keycode);
            }
            break;
        case XNX_EVENT_POINTER:
            break;
        default:
            break;
        }
    }
}

void DG_DrawFrame() {
    if (s_xnx && DG_ScreenBuffer) {
        xnx_write_buffer(s_xnx, s_surface_id, 0, 0,
                         DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer);
        xnx_commit(s_xnx, s_surface_id);
    }
    handleXnxEvents();
}

void DG_SleepMs(uint32_t ms) {
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
    handleXnxEvents();

    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) return 0;

    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex = (s_KeyQueueReadIndex + 1) % KEYQUEUE_SIZE;
    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;
    return 1;
}

void DG_SetWindowTitle(const char *title) {
    if (s_xnx && s_surface_id && title) {
        xnx_set_title(s_xnx, s_surface_id, title);
    }
}

extern int myargc;
extern char **myargv;

int main(int argc, char **argv) {
    doomgeneric_Create(argc, argv);
    while (1) {
        doomgeneric_Tick();
    }
    return 0;
}
