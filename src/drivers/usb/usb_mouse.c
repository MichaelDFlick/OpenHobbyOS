#include "usb.h"
#include "uhci.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "pit.h"

#define HID_REQ_GET_REPORT    0x01
#define HID_REQ_SET_PROTOCOL  0x0B
#define HID_PROTOCOL_BOOT     0

typedef struct {
    u8 buttons;
    i8 dx;
    i8 dy;
} usb_mouse_report_t;

static usb_mouse_report_t last_report;
static bool mouse_initialized;
static bool report_changed;

static void usb_mouse_probe(usb_device_t *dev) {
    if (mouse_initialized) {
        console_printf("[usb_mouse] mouse already active\n");
        return;
    }

    u8 *cfg = dev->config_raw;
    u8 len = dev->config_length;
    u8 iface_num = 0;
    u8 ep_in = 0;
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
                iface->bInterfaceProtocol == 2) {
                iface_num = iface->bInterfaceNumber;
                found = true;
            }
        } else if (desc_type == USB_DESC_ENDPOINT && found) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&cfg[offset];
            if ((ep->bEndpointAddress & USB_ENDPOINT_IN) && ep->wMaxPacketSize > 0) {
                ep_in = ep->bEndpointAddress;
            }
        }
        offset += desc_len;
    }

    if (!found) {
        console_printf("[usb_mouse] no HID mouse interface found\n");
        return;
    }

    usb_control_transfer(dev, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                         HID_REQ_SET_PROTOCOL, HID_PROTOCOL_BOOT, iface_num, 0, NULL);
    pit_sleep(1);

    console_printf("[usb_mouse] USB mouse at addr=%d iface=%d ep=%02x\n",
                   dev->address, iface_num, ep_in);

    u8 report[4];
    memset(report, 0, 4);
    usb_control_transfer(dev, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                         HID_REQ_GET_REPORT, 0x0100, iface_num, 4, report);

    last_report.buttons = report[0] & 0x07;
    last_report.dx = (i8)report[1];
    last_report.dy = (i8)report[2];

    mouse_initialized = true;
    console_printf("[usb_mouse] USB mouse ready\n");
}

void usb_mouse_poll(void) {
    if (!mouse_initialized) return;
}

bool usb_mouse_has_event(void) {
    return report_changed;
}

usb_mouse_report_t usb_mouse_read(void) {
    report_changed = false;
    return last_report;
}

static const usb_class_driver_t mouse_driver = {
    .class = USB_CLASS_HID,
    .subclass = 1,
    .protocol = 2,
    .probe = usb_mouse_probe,
};

void usb_mouse_init(void) {
    usb_class_driver_register(&mouse_driver);
    console_printf("[usb_mouse] HID mouse driver registered\n");
}
