#include "mouse.h"

#include "idt.h"
#include "io.h"
#include "pic.h"

#define PS2_CMD_PORT 0x64
#define PS2_DATA_PORT 0x60
#define MOUSE_BUFFER_SIZE 64

static volatile mouse_event_t buffer[MOUSE_BUFFER_SIZE];
static volatile u32 head;
static volatile u32 tail;

static u8 mouse_cycle;
static u8 mouse_packet[4];
static u8 mouse_has_wheel;

static int ps2_send_cmd(u8 cmd) {
    int timeout = 100000;
    while ((inb(PS2_CMD_PORT) & 2) && timeout--) {
        cpu_pause();
    }
    if (!timeout) return -1;
    outb(PS2_CMD_PORT, cmd);
    return 0;
}

static int ps2_send_data(u8 data) {
    int timeout = 100000;
    while ((inb(PS2_CMD_PORT) & 2) && timeout--) {
        cpu_pause();
    }
    if (!timeout) return -1;
    outb(PS2_DATA_PORT, data);
    return 0;
}

static int mouse_send_cmd(u8 cmd) {
    if (ps2_send_cmd(0xD4) < 0) return -1;
    return ps2_send_data(cmd);
}

static u8 ps2_read_data(void) {
    int timeout = 100000;
    while (!(inb(PS2_CMD_PORT) & 1) && timeout--) {
        cpu_pause();
    }
    return inb(PS2_DATA_PORT);
}

static void mouse_irq(UNUSED registers_t *regs) {
    u8 data = inb(PS2_DATA_PORT);

    if (mouse_cycle == 0) {
        if (!(data & 0x08)) {
            return;
        }
        mouse_packet[0] = data;
        mouse_cycle = 1;
        return;
    }

    if (mouse_cycle == 1) {
        mouse_packet[1] = data;
        mouse_cycle = 2;
        return;
    }

    if (mouse_cycle == 2) {
        mouse_packet[2] = data;
        mouse_cycle = 0;

        mouse_event_t ev;
        ev.buttons = mouse_packet[0] & 0x07;
        ev.dx = (i8)(mouse_packet[1]) - (i8)((mouse_packet[0] << 4) & 0x100);
        ev.dy = (i8)(mouse_packet[2]) - (i8)((mouse_packet[0] << 3) & 0x100);

        if (mouse_has_wheel && mouse_cycle == 3) {
            mouse_packet[3] = data;
            mouse_cycle = 3;
        }

        u32 next = (head + 1) % MOUSE_BUFFER_SIZE;
        if (next != tail) {
            buffer[head] = ev;
            head = next;
        }
    }
}

void mouse_init(void) {
    head = 0;
    tail = 0;
    mouse_cycle = 0;

    ps2_send_cmd(0xA8);
    ps2_read_data();

    ps2_send_cmd(0x20);
    u8 config = ps2_read_data();
    config |= (1 << 1);
    config &= ~(1 << 5);
    ps2_send_cmd(0x60);
    ps2_send_data(config);

    mouse_send_cmd(0xF6);
    ps2_read_data();

    mouse_send_cmd(0xF4);
    if (ps2_read_data() == 0xFA) {
        mouse_has_wheel = 0;
    }

    irq_install_handler(12, mouse_irq);
    pic_clear_mask(12);
}

bool mouse_has_event(void) {
    return head != tail;
}

mouse_event_t mouse_read_event(void) {
    while (head == tail) {
        cpu_halt();
    }
    mouse_event_t ev = buffer[tail];
    tail = (tail + 1) % MOUSE_BUFFER_SIZE;
    return ev;
}
