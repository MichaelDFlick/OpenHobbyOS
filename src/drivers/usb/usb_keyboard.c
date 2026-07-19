#include "usb.h"
#include "uhci.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "pit.h"
#include "keyboard.h"

#define HID_REQ_GET_REPORT    0x01
#define HID_REQ_SET_PROTOCOL  0x0B
#define HID_REQ_SET_IDLE      0x0A

#define HID_PROTOCOL_BOOT     0

#define KEYBUF_SIZE 64

static u8 keybuf[KEYBUF_SIZE];
static u8 keybuf_head;
static u8 keybuf_tail;

static bool keyboard_initialized;

static const u8 usb_keycode_to_ascii[][2] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {'a', 'A'}, {'b', 'B'}, {'c', 'C'}, {'d', 'D'},
    {'e', 'E'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'},
    {'i', 'I'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'},
    {'m', 'M'}, {'n', 'N'}, {'o', 'O'}, {'p', 'P'},
    {'q', 'Q'}, {'r', 'R'}, {'s', 'S'}, {'t', 'T'},
    {'u', 'U'}, {'v', 'V'}, {'w', 'W'}, {'x', 'X'},
    {'y', 'Y'}, {'z', 'Z'}, {'1', '!'}, {'2', '@'},
    {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'},
    {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'},
    {'\n', '\n'}, {0, 0}, {'\b', '\b'}, {0, 0},
    {' ', ' '}, {'-', '_'}, {'=', '+'}, {'[', '{'},
    {']', '}'}, {'\\', '|'}, {0, 0}, {';', ':'},
    {'\'', '"'}, {'`', '~'}, {',', '<'}, {'.', '>'},
    {'/', '?'},
};

static void push_key(char ch) {
    u8 next = (keybuf_head + 1) % KEYBUF_SIZE;
    if (next != keybuf_tail) {
        keybuf[keybuf_head] = (u8)ch;
        keybuf_head = next;
    }
}

static u8 get_key(void) {
    if (keybuf_tail == keybuf_head) return 0;
    u8 ch = keybuf[keybuf_tail];
    keybuf_tail = (keybuf_tail + 1) % KEYBUF_SIZE;
    return ch;
}

static void process_report(u8 *report) {
    u8 modifiers = report[0];
    bool shift = (modifiers & 0x02) || (modifiers & 0x20);
    static u8 prev_keys[6];
    static bool first_report = true;

    if (first_report) {
        memcpy(prev_keys, report + 2, 6);
        first_report = false;
        return;
    }

    for (int i = 0; i < 6; i++) {
        u8 key = report[2 + i];
        if (key == 0) break;

        bool was_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (prev_keys[j] == key) {
                was_pressed = true;
                break;
            }
        }

        if (!was_pressed && key < sizeof(usb_keycode_to_ascii) / sizeof(usb_keycode_to_ascii[0])) {
            char ch = usb_keycode_to_ascii[key][shift ? 1 : 0];
            if (ch) {
                /* Push to both our local buffer and the PS/2 keyboard buffer */
                push_key(ch);
                keyboard_push_char(ch);
            }
        }
    }

    memcpy(prev_keys, report + 2, 6);
}

static void usb_keyboard_probe(usb_device_t *dev) {
    if (keyboard_initialized) {
        console_printf("[usb_kbd] keyboard already active\n");
        return;
    }

    u8 *cfg = dev->config_raw;
    u8 len = dev->config_length;
    u8 iface_num = 0;
    u8 ep_in = 0;
    u16 ep_max = 0;
    u8 ep_interval = 0;
    bool found = false;

    u8 offset = 0;
    while (offset < len) {
        u8 desc_len = cfg[offset];
        u8 desc_type = cfg[offset + 1];
        if (desc_len == 0) break;

        if (desc_type == USB_DESC_INTERFACE) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)&cfg[offset];
            if (iface->bInterfaceClass == USB_CLASS_HID &&
                iface->bInterfaceSubClass == 1 &&
                iface->bInterfaceProtocol == 1) {
                iface_num = iface->bInterfaceNumber;
                found = true;
            }
        } else if (desc_type == USB_DESC_ENDPOINT && found) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&cfg[offset];
            if ((ep->bEndpointAddress & USB_ENDPOINT_IN) && ep->wMaxPacketSize > 0) {
                ep_in = ep->bEndpointAddress;
                ep_max = ep->wMaxPacketSize;
                ep_interval = ep->bInterval;
            }
        }
        offset += desc_len;
    }

    if (!found) {
        console_printf("[usb_kbd] no HID keyboard interface found\n");
        return;
    }

    usb_control_transfer(dev, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                         HID_REQ_SET_PROTOCOL, HID_PROTOCOL_BOOT, iface_num, 0, NULL);
    pit_sleep(1);

    console_printf("[usb_kbd] USB keyboard at addr=%d iface=%d ep=%02x\n",
                   dev->address, iface_num, ep_in);

    u8 report[8];
    memset(report, 0, 8);

    for (int poll = 0; poll < 5; poll++) {
        usb_control_transfer(dev, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                             HID_REQ_GET_REPORT, 0x0100, iface_num, 8, report);
        process_report(report);
        pit_sleep(1);
    }

    keyboard_initialized = true;
    console_printf("[usb_kbd] USB keyboard ready\n");
}

static const usb_class_driver_t kbd_driver = {
    .class = USB_CLASS_HID,
    .subclass = 1,
    .protocol = 1,
    .probe = usb_keyboard_probe,
};

void usb_keyboard_init(void) {
    usb_class_driver_register(&kbd_driver);
    console_printf("[usb_kbd] HID keyboard driver registered\n");
}
