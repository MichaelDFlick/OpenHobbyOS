#include "uhci.h"
#include "pci.h"
#include "io.h"
#include "idt.h"
#include "pic.h"
#include "memory.h"
#include "console.h"
#include "string.h"
#include "pit.h"

#define FRAME_LIST_SIZE (1024 * 4)

static usb_hcd_ops_t uhci_ops;
static uhci_dev_t uhci_dev;

static u32 virt_to_phys(void *virt) {
    return (u32)(uintptr_t)virt;
}

static void uhci_reset(uhci_dev_t *dev) {
    outw(dev->io_base + UHCI_CMD, UHCI_CMD_HCRESET);
    for (int i = 0; i < 10000; i++) {
        io_wait();
        if (!(inw(dev->io_base + UHCI_CMD) & UHCI_CMD_HCRESET)) break;
    }
}

static void uhci_stop(uhci_dev_t *dev) {
    outw(dev->io_base + UHCI_CMD, inw(dev->io_base + UHCI_CMD) & ~UHCI_CMD_RS);
    for (int i = 0; i < 10000; i++) {
        io_wait();
        if (inw(dev->io_base + UHCI_STS) & UHCI_STS_HCH) break;
    }
}

static void uhci_start(uhci_dev_t *dev) {
    outw(dev->io_base + UHCI_CMD, inw(dev->io_base + UHCI_CMD) | UHCI_CMD_RS);
}

static void uhci_td_init(uhci_td_t *td, u32 link, u32 token, u32 buffer) {
    td->link = link;
    td->status = UHCI_TD_ACTIVE;
    td->token = token;
    td->buffer = buffer;
    td->buffer_hi = 0;
    td->reserved[0] = 0;
    td->reserved[1] = 0;
    td->reserved[2] = 0;
}

static int uhci_control_transfer(usb_device_t *usb_dev, usb_setup_packet_t *req, void *data) {
    uhci_dev_t *dev = (uhci_dev_t *)usb_dev->hc_private;
    if (!dev || !dev->initialized) return -1;

    u16 max0 = usb_dev->desc.bMaxPacketSize0;
    if (max0 == 0) max0 = 8;

    u16 data_length = req->wLength;
    bool has_data = (data_length > 0 && data != NULL);
    u8 pid_data = (req->bmRequestType & USB_DIR_IN) ? UHCI_PID_IN : UHCI_PID_OUT;

    int num_tds = has_data ? 3 : 2;
    uhci_td_t *td = &dev->async_tds[0];
    u32 td_phys = dev->async_td_phys;
    u32 buf_phys = 0;

    if (has_data) {
        buf_phys = virt_to_phys(data);
    }

    u32 setup_token = (usb_dev->address & 0x7F) | (0 << 7) | (0 << 14) | (UHCI_PID_SETUP << 16);
    u32 setup_link;
    if (has_data) {
        setup_link = (td_phys + sizeof(uhci_td_t)) | 2;
    } else {
        setup_link = (td_phys + 2 * sizeof(uhci_td_t)) | 2;
    }
    uhci_td_init(&td[0], setup_link, setup_token, virt_to_phys(req));
    td[0].status = UHCI_TD_ACTIVE;

    if (has_data) {
        u32 data_token = (usb_dev->address & 0x7F) | (0 << 7) | (1 << 14) | (pid_data << 16);
        if (data_length > max0) data_length = max0;
        u32 data_link = (td_phys + 2 * sizeof(uhci_td_t)) | 2;
        uhci_td_init(&td[1], data_link, data_token, buf_phys);
        td[1].status = UHCI_TD_ACTIVE;
    }

    u8 pid_status = (req->bmRequestType & USB_DIR_IN) ? UHCI_PID_OUT : UHCI_PID_IN;
    u32 status_token = (usb_dev->address & 0x7F) | (0 << 7) | (1 << 14) | (pid_status << 16);
    int status_idx = has_data ? 2 : 1;
    uhci_td_init(&td[status_idx], 1, status_token, 0);
    td[status_idx].status = UHCI_TD_ACTIVE;

    dev->async_qh->elp = td_phys;
    outw(dev->io_base + UHCI_STS, 0x003F);
    uhci_start(dev);

    u32 timeout = 50000;
    while (timeout--) {
        if (!(td[0].status & UHCI_TD_ACTIVE)) break;
        io_wait();
    }
    if (timeout == 0) {
        console_printf("[uhci] control transfer timed out\n");
        dev->async_qh->elp = 1;
        return -1;
    }

    dev->async_qh->elp = 1;

    if (td[0].status & UHCI_TD_STALLED) {
        console_printf("[uhci] control transfer stalled\n");
        return -1;
    }
    if (td[0].status & UHCI_TD_BUFERR) {
        return -1;
    }
    return 0;
}

static int uhci_interrupt_transfer(usb_device_t *dev, u8 endpoint, void *data, u16 length) {
    (void)dev;
    (void)endpoint;
    (void)data;
    (void)length;
    return -1;
}

static int uhci_bulk_transfer(usb_device_t *dev, u8 endpoint, void *data, u16 length) {
    (void)dev;
    (void)endpoint;
    (void)data;
    (void)length;
    return -1;
}

static int uhci_reset_port(usb_device_t *hub, u8 port) {
    (void)hub;
    uhci_dev_t *dev = &uhci_dev;
    if (port < 1 || port > dev->num_ports) return -1;
    u16 reg = (port == 1) ? UHCI_PORTSC1 : UHCI_PORTSC2;
    outw(dev->io_base + reg, inw(dev->io_base + reg) | UHCI_PORTSC_RESET);
    pit_sleep(5);
    outw(dev->io_base + reg, inw(dev->io_base + reg) & ~UHCI_PORTSC_RESET);
    pit_sleep(2);
    return 0;
}

static void uhci_irq_handler(registers_t *regs) {
    (void)regs;
    uhci_dev_t *dev = &uhci_dev;
    u16 sts = inw(dev->io_base + UHCI_STS);
    if (!sts) return;
    if (sts & UHCI_STS_USBINT) {
    }
    if (sts & UHCI_STS_ERROR) {
    }
    outw(dev->io_base + UHCI_STS, sts);
}

static void uhci_probe_port(uhci_dev_t *dev, int port, void *hc_private) {
    u16 reg = (port == 1) ? UHCI_PORTSC1 : UHCI_PORTSC2;
    u16 portsc = inw(dev->io_base + reg);

    console_printf("[uhci] port %d: status=%04x\n", port, portsc);

    if (!(portsc & UHCI_PORTSC_CCS)) {
        console_printf("[uhci] port %d: no device\n", port);
        return;
    }

    outw(dev->io_base + reg, portsc | UHCI_PORTSC_RESET);
    pit_sleep(5);
    outw(dev->io_base + reg, (portsc | UHCI_PORTSC_PE) & ~UHCI_PORTSC_RESET);
    pit_sleep(2);

    portsc = inw(dev->io_base + reg);
    if (!(portsc & UHCI_PORTSC_CCS)) {
        console_printf("[uhci] port %d: device disconnected during reset\n", port);
        return;
    }

    if (!(portsc & UHCI_PORTSC_PE)) {
        outw(dev->io_base + reg, portsc | UHCI_PORTSC_PE);
        pit_sleep(1);
        portsc = inw(dev->io_base + reg);
        if (!(portsc & UHCI_PORTSC_PE)) {
            console_printf("[uhci] port %d: failed to enable\n", port);
            return;
        }
    }

    u8 speed = (portsc & UHCI_PORTSC_LSDA) ? 1 : 0;
    console_printf("[uhci] port %d: device connected (speed=%s)\n", port, speed ? "low" : "full");

    usb_device_t usb_dev;
    memset(&usb_dev, 0, sizeof(usb_dev));
    usb_dev.port = port;
    usb_dev.speed = speed;
    usb_dev.hc_private = hc_private;

    int ret = usb_get_descriptor(&usb_dev, USB_DESC_DEVICE, 0, &usb_dev.desc, 8);
    if (ret < 0) {
        console_printf("[uhci] port %d: failed to get device descriptor\n", port);
        return;
    }

    u8 max0 = usb_dev.desc.bMaxPacketSize0;

    ret = usb_get_descriptor(&usb_dev, USB_DESC_DEVICE, 0, &usb_dev.desc, sizeof(usb_device_descriptor_t));
    if (ret < 0) {
        console_printf("[uhci] port %d: failed to get full device descriptor\n", port);
        return;
    }
    usb_dev.desc.bMaxPacketSize0 = max0;

    int addr = usb_device_add(&usb_dev);
    if (addr < 0) {
        console_printf("[uhci] port %d: failed to allocate device address\n", port);
        return;
    }

    usb_dev.address = addr;
    memset(&usb_dev.desc, 0, sizeof(usb_device_descriptor_t));
    usb_dev.desc.bMaxPacketSize0 = max0;

    ret = usb_get_descriptor(&usb_dev, USB_DESC_DEVICE, 0, &usb_dev.desc, sizeof(usb_device_descriptor_t));
    if (ret < 0) {
        usb_device_remove(addr);
        return;
    }

    usb_set_address(&usb_dev, addr);
    pit_sleep(1);

    usb_dev.address = addr;
    usb_dev.desc.bMaxPacketSize0 = max0;

    ret = usb_get_descriptor(&usb_dev, USB_DESC_DEVICE, 0, &usb_dev.desc, sizeof(usb_device_descriptor_t));
    if (ret < 0) {
        console_printf("[uhci] port %d: failed to re-read device descriptor\n", port);
        usb_device_remove(addr);
        return;
    }

    usb_device_t *registered = usb_device_get(addr);
    if (registered) {
        registered->desc = usb_dev.desc;
        registered->speed = speed;
        registered->port = port;
        registered->hc_private = hc_private;
    }

    console_printf("[uhci] port %d: device addr=%d class=%02x subclass=%02x proto=%02x vendor=%04x product=%04x\n",
                   port, addr, usb_dev.desc.bDeviceClass, usb_dev.desc.bDeviceSubClass,
                   usb_dev.desc.bDeviceProtocol, usb_dev.desc.idVendor, usb_dev.desc.idProduct);

    ret = usb_get_descriptor(registered, USB_DESC_CONFIG, 0, registered->config_raw,
                              sizeof(registered->config_raw));
    if (ret < 0) {
        console_printf("[uhci] port %d: failed to get config descriptor\n", port);
        return;
    }
    registered->config_length = (u8)ret;

    usb_set_configuration(registered, 1);
    pit_sleep(1);
    registered->configuration = 1;
    registered->configured = true;

    usb_probe_device(registered);
}

void uhci_enum_ports(void *dev_void) {
    uhci_dev_t *dev = (uhci_dev_t *)dev_void;
    for (int port = 1; port <= dev->num_ports; port++) {
        uhci_probe_port(dev, port, dev_void);
    }
}

void *uhci_init(const pci_device_t *pci) {
    uhci_dev_t *dev = &uhci_dev;
    memset(dev, 0, sizeof(uhci_dev_t));

    dev->io_base = (u16)(pci->bar0 & 0xFFFC);
    dev->irq = pci->irq;
    dev->num_ports = 2;

    u32 cmd = pci_config_read(pci->bus, pci->slot, pci->func, 0x04);
    cmd |= 0x05;
    cmd &= ~0x04;
    pci_config_write(pci->bus, pci->slot, pci->func, 0x04, cmd);

    uhci_reset(dev);
    uhci_stop(dev);

    dev->frame_list = (u32 *)kmalloc_aligned(FRAME_LIST_SIZE, 4096);
    if (!dev->frame_list) {
        console_printf("[uhci] failed to allocate frame list\n");
        return NULL;
    }
    dev->frame_list_phys = virt_to_phys(dev->frame_list);
    memset(dev->frame_list, 0, FRAME_LIST_SIZE);

    dev->async_qh = (uhci_qh_t *)kmalloc_aligned(32, 16);
    if (!dev->async_qh) {
        kfree(dev->frame_list);
        return NULL;
    }
    dev->async_qh_phys = virt_to_phys(dev->async_qh);
    memset(dev->async_qh, 0, 32);
    dev->async_qh->hlp = dev->async_qh_phys | 2;
    dev->async_qh->elp = 1;

    dev->async_tds = (uhci_td_t *)kmalloc_aligned(64 * sizeof(uhci_td_t), 16);
    if (!dev->async_tds) {
        kfree(dev->async_qh);
        kfree(dev->frame_list);
        return NULL;
    }
    dev->async_td_phys = virt_to_phys(dev->async_tds);
    memset(dev->async_tds, 0, 64 * sizeof(uhci_td_t));

    for (int i = 0; i < 1024; i++) {
        dev->frame_list[i] = dev->async_qh_phys | 2;
    }

    outl(dev->io_base + UHCI_FLBASEADD, dev->frame_list_phys);
    outb(dev->io_base + UHCI_SOFMOD, 64);
    outw(dev->io_base + UHCI_FRNUM, 0);
    outw(dev->io_base + UHCI_STS, 0x003F);
    outw(dev->io_base + UHCI_CMD, UHCI_CMD_RS | UHCI_CMD_CF | UHCI_CMD_MAXP);

    dev->initialized = true;

    console_printf("[uhci] initialized: io=%04x irq=%u frame_list=%08x\n",
                   dev->io_base, dev->irq, dev->frame_list_phys);

    return dev;
}

usb_hcd_ops_t *uhci_get_ops(void) {
    uhci_ops.control_transfer = uhci_control_transfer;
    uhci_ops.interrupt_transfer = uhci_interrupt_transfer;
    uhci_ops.bulk_transfer = uhci_bulk_transfer;
    uhci_ops.reset_port = uhci_reset_port;
    return &uhci_ops;
}
