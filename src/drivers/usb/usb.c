#include "usb.h"
#include "uhci.h"
#include "pci.h"
#include "console.h"
#include "string.h"
#include "memory.h"

static usb_device_t usb_devices[USB_MAX_DEVICES];
static u8 next_address = 1;

static usb_hcd_ops_t *hcd_ops;
static void *hcd_private;

static usb_class_driver_t class_drivers[USB_MAX_CLASS_DRIVERS];
static u8 num_class_drivers;

int usb_class_driver_register(const usb_class_driver_t *driver) {
    if (num_class_drivers >= USB_MAX_CLASS_DRIVERS) return -1;
    class_drivers[num_class_drivers++] = *driver;
    console_printf("[usb] class driver registered: class=%02x subclass=%02x protocol=%02x\n",
                   driver->class, driver->subclass, driver->protocol);
    return 0;
}

void usb_probe_device(usb_device_t *dev) {
    for (u8 i = 0; i < num_class_drivers; i++) {
        if (class_drivers[i].class == dev->desc.bDeviceClass &&
            (class_drivers[i].subclass == 0xFF || class_drivers[i].subclass == dev->desc.bDeviceSubClass) &&
            (class_drivers[i].protocol == 0xFF || class_drivers[i].protocol == dev->desc.bDeviceProtocol)) {
            class_drivers[i].probe(dev);
            return;
        }
    }
    console_printf("[usb] no driver for device class=%02x subclass=%02x protocol=%02x\n",
                   dev->desc.bDeviceClass, dev->desc.bDeviceSubClass, dev->desc.bDeviceProtocol);
}

int usb_init(void) {
    memset(usb_devices, 0, sizeof(usb_devices));

    const pci_device_t *pci = pci_find_by_class(PCI_CLASS_SERIAL_BUS, PCI_SUBCLASS_USB);
    if (!pci) {
        console_printf("[usb] no host controller found\n");
        return -1;
    }

    console_printf("[usb] host controller: %04x:%04x (prog_if=%02x) bus=%d slot=%d func=%d\n",
                   pci->vendor_id, pci->device_id, pci->prog_if,
                   pci->bus, pci->slot, pci->func);

    if (pci->prog_if == PCI_PROGIF_UHCI) {
        hcd_private = uhci_init(pci);
        if (!hcd_private) {
            console_printf("[usb] UHCI init failed\n");
            return -1;
        }
        hcd_ops = uhci_get_ops();
        console_printf("[usb] UHCI host controller initialized\n");
    } else {
        console_printf("[usb] unsupported host controller type (prog_if=%02x)\n", pci->prog_if);
        return -1;
    }

    uhci_enum_ports(hcd_private);
    return 0;
}

int usb_device_add(usb_device_t *dev) {
    if (next_address > USB_MAX_DEVICES) return -1;
    u8 addr = next_address++;
    dev->address = addr;
    memcpy(&usb_devices[addr], dev, sizeof(usb_device_t));
    console_printf("[usb] device %d: class=%02x subclass=%02x proto=%02x vendor=%04x product=%04x\n",
                   addr, dev->desc.bDeviceClass, dev->desc.bDeviceSubClass,
                   dev->desc.bDeviceProtocol, dev->desc.idVendor, dev->desc.idProduct);
    return addr;
}

void usb_device_remove(u8 address) {
    if (address < USB_MAX_DEVICES) {
        memset(&usb_devices[address], 0, sizeof(usb_device_t));
    }
}

usb_device_t *usb_device_get(u8 address) {
    if (address < USB_MAX_DEVICES && usb_devices[address].address == address) {
        return &usb_devices[address];
    }
    return NULL;
}

int usb_control_transfer(usb_device_t *dev, u8 bmReqType, u8 bRequest,
                         u16 wValue, u16 wIndex, u16 wLength, void *data) {
    if (!hcd_ops || !hcd_ops->control_transfer) return -1;
    usb_setup_packet_t req;
    req.bmRequestType = bmReqType;
    req.bRequest = bRequest;
    req.wValue = wValue;
    req.wIndex = wIndex;
    req.wLength = wLength;
    return hcd_ops->control_transfer(dev, &req, data);
}

int usb_get_descriptor(usb_device_t *dev, u8 type, u8 index, void *buf, u16 length) {
    return usb_control_transfer(dev, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                USB_REQ_GET_DESCRIPTOR, (u16)((type << 8) | index), 0, length, buf);
}

int usb_set_address(usb_device_t *dev, u8 address) {
    return usb_control_transfer(dev, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                USB_REQ_SET_ADDRESS, address, 0, 0, NULL);
}

int usb_set_configuration(usb_device_t *dev, u8 config) {
    return usb_control_transfer(dev, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                                USB_REQ_SET_CONFIGURATION, config, 0, 0, NULL);
}

int usb_clear_halt(usb_device_t *dev, u8 endpoint) {
    return usb_control_transfer(dev, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT,
                                USB_REQ_CLEAR_FEATURE, 0, endpoint, 0, NULL);
}

int usb_reset_port(usb_device_t *hub, u8 port) {
    if (!hcd_ops || !hcd_ops->reset_port) return -1;
    return hcd_ops->reset_port(hub, port);
}
