#include "usb.h"
#include "uhci.h"
#include "console.h"
#include "string.h"
#include "memory.h"
#include "pit.h"

#define USB_REQ_GET_STATUS      0x00
#define USB_REQ_CLEAR_FEATURE   0x01
#define USB_REQ_SET_FEATURE     0x03

#define FEATURE_PORT_RESET      4
#define FEATURE_PORT_POWER      8

typedef struct PACKED {
    u8  bDescLength;
    u8  bDescriptorType;
    u8  bNbrPorts;
    u16 wHubCharacteristics;
    u8  bPwrOn2PwrGood;
    u8  bHubContrCurrent;
    u8  DeviceRemovable[1];
    u8  PortPwrCtrlMask[1];
} usb_hub_descriptor_t;

static int usb_hub_control(usb_device_t *dev, u8 bmReqType, u8 bRequest,
                           u16 wValue, u16 wIndex, u16 wLength, void *data) {
    return usb_control_transfer(dev, bmReqType, bRequest, wValue, wIndex, wLength, data);
}

static void usb_hub_probe(usb_device_t *dev) {
    usb_hub_descriptor_t hub_desc;
    int ret = usb_hub_control(dev, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE,
                               USB_REQ_GET_STATUS, 0, 0, sizeof(hub_desc), &hub_desc);
    if (ret < 0) {
        console_printf("[usb_hub] failed to get hub descriptor\n");
        return;
    }

    u8 num_ports = hub_desc.bNbrPorts;
    console_printf("[usb_hub] %d ports, pwr_on=%dms\n", num_ports, hub_desc.bPwrOn2PwrGood * 2);

    for (u8 port = 1; port <= num_ports; port++) {
        usb_hub_control(dev, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_OTHER,
                        USB_REQ_SET_FEATURE, FEATURE_PORT_POWER, port, 0, NULL);
    }

    if (hub_desc.bPwrOn2PwrGood > 0) {
        pit_sleep(hub_desc.bPwrOn2PwrGood);
    }

    for (u8 port = 1; port <= num_ports; port++) {
        u32 port_status;
        ret = usb_hub_control(dev, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_OTHER,
                              USB_REQ_GET_STATUS, 0, port, 4, &port_status);
        if (ret < 0) continue;

        bool connected = (port_status & 0x01) != 0;
        bool changed = (port_status & 0x10000) != 0;

        console_printf("[usb_hub] port %d: status=%08x\n", port, port_status);

        if (!connected) continue;

        usb_hub_control(dev, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_OTHER,
                        USB_REQ_SET_FEATURE, FEATURE_PORT_RESET, port, 0, NULL);
        pit_sleep(2);

        ret = usb_hub_control(dev, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_OTHER,
                              USB_REQ_GET_STATUS, 0, port, 4, &port_status);
        if (ret < 0) continue;

        if (!(port_status & 0x01)) {
            console_printf("[usb_hub] port %d: device disconnected during reset\n", port);
            continue;
        }

        u8 speed = (port_status & 0x0200) ? 1 : 0;
        console_printf("[usb_hub] port %d: device ready (speed=%s)\n", port, speed ? "low" : "full");

        usb_device_t child_dev;
        memset(&child_dev, 0, sizeof(child_dev));
        child_dev.port = port;
        child_dev.speed = speed;
        child_dev.hub = dev;
        child_dev.hc_private = dev->hc_private;

        ret = usb_get_descriptor(&child_dev, USB_DESC_DEVICE, 0, &child_dev.desc, 8);
        if (ret < 0) {
            console_printf("[usb_hub] port %d: failed to get device descriptor\n", port);
            continue;
        }

        u8 max0 = child_dev.desc.bMaxPacketSize0;
        ret = usb_get_descriptor(&child_dev, USB_DESC_DEVICE, 0, &child_dev.desc,
                                  sizeof(usb_device_descriptor_t));
        if (ret < 0) continue;
        child_dev.desc.bMaxPacketSize0 = max0;

        int addr = usb_device_add(&child_dev);
        if (addr < 0) continue;

        usb_set_address(usb_device_get((u8)addr), (u8)addr);
        pit_sleep(1);

        usb_device_t *registered = usb_device_get((u8)addr);
        if (!registered) continue;

        registered->hub = dev;
        registered->hc_private = dev->hc_private;
        registered->desc.bMaxPacketSize0 = max0;

        ret = usb_get_descriptor(registered, USB_DESC_DEVICE, 0, &registered->desc,
                                  sizeof(usb_device_descriptor_t));
        if (ret < 0) continue;

        ret = usb_get_descriptor(registered, USB_DESC_CONFIG, 0, registered->config_raw,
                                  sizeof(registered->config_raw));
        if (ret < 0) continue;
        registered->config_length = (u8)ret;

        usb_set_configuration(registered, 1);
        pit_sleep(1);
        registered->configuration = 1;
        registered->configured = true;

        usb_probe_device(registered);
    }
}

static const usb_class_driver_t hub_driver = {
    .class = USB_CLASS_HUB,
    .subclass = 0xFF,
    .protocol = 0xFF,
    .probe = usb_hub_probe,
};

void usb_hub_init(void) {
    usb_class_driver_register(&hub_driver);
    console_printf("[usb_hub] hub driver registered\n");
}
