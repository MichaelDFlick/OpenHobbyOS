#ifndef OHOS_MOUSE_H
#define OHOS_MOUSE_H

#include "types.h"

#define MOUSE_PACKET_SIZE 3

typedef struct {
    u8 buttons;
    i8 dx;
    i8 dy;
} mouse_event_t;

void mouse_init(void);
bool mouse_has_event(void);
mouse_event_t mouse_read_event(void);

#endif
