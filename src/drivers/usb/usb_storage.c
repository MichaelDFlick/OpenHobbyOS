#include "usb.h"
#include "uhci.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "pit.h"
#include "blkdev.h"

#define BOT_CBW_SIGNATURE    0x43425355
#define BOT_CSW_SIGNATURE    0x53425355

#define BOT_CBW_LEN      31
#define BOT_CSW_LEN      13

#define SCSI_TEST_UNIT_READY   0x00
#define SCSI_INQUIRY           0x12
#define SCSI_READ_CAPACITY_10  0x25
#define SCSI_READ_10           0x28
#define SCSI_WRITE_10          0x2A

typedef struct PACKED {
    u32 signature;
    u32 tag;
    u32 data_length;
    u8  flags;
    u8  lun;
    u8  cb_length;
    u8  cdb[16];
} bot_cbw_t;

typedef struct PACKED {
    u32 signature;
    u32 tag;
    u32 residue;
    u8  status;
} bot_csw_t;

typedef struct {
    usb_device_t *usb_dev;
    u8  ep_in;
    u8  ep_out;
    u16 ep_max_in;
    u16 ep_max_out;
    u32 tag;
    u32 total_sectors;
    bool initialized;
    u8  blkdev_id;
    bool blkdev_registered;
} usb_storage_dev_t;

static usb_storage_dev_t stor_dev;

static int bot_transport(usb_storage_dev_t *sd, u8 *cdb, u8 cdb_len,
                         u32 data_length, u8 *data, u8 dir_in) {
    bot_cbw_t *cbw = kcalloc(1, BOT_CBW_LEN);
    if (!cbw) return -1;
    cbw->signature = BOT_CBW_SIGNATURE;
    cbw->tag = sd->tag++;
    cbw->data_length = data_length;
    cbw->flags = dir_in ? 0x80 : 0x00;
    cbw->lun = 0;
    cbw->cb_length = cdb_len;
    memcpy(cbw->cdb, cdb, cdb_len);

    int ret = usb_control_transfer(sd->usb_dev, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                                    0x00, 0, 0, 0, NULL);
    if (ret < 0) {
        ret = usb_clear_halt(sd->usb_dev, sd->ep_out);
        if (ret < 0) {
            kfree(cbw);
            return -1;
        }
    }

    ret = uhci_get_ops()->bulk_transfer(sd->usb_dev, sd->ep_out, cbw, BOT_CBW_LEN);
    if (ret < 0) {
        kfree(cbw);
        return -1;
    }

    if (data_length > 0 && data != NULL) {
        u8 ep = dir_in ? sd->ep_in : sd->ep_out;
        ret = uhci_get_ops()->bulk_transfer(sd->usb_dev, ep, data, (u16)data_length);
        if (ret < 0) {
            kfree(cbw);
            return -1;
        }
    }

    bot_csw_t *csw = kcalloc(1, BOT_CSW_LEN);
    if (!csw) {
        kfree(cbw);
        return -1;
    }

    ret = uhci_get_ops()->bulk_transfer(sd->usb_dev, sd->ep_in, csw, BOT_CSW_LEN);
    if (ret < 0) {
        kfree(cbw);
        kfree(csw);
        return -1;
    }

    kfree(cbw);

    if (csw->signature != BOT_CSW_SIGNATURE) {
        console_printf("[usb_stor] bad CSW signature: %08x\n", csw->signature);
        kfree(csw);
        return -1;
    }

    u8 status = csw->status;
    kfree(csw);

    if (status != 0) return -1;
    return 0;
}

static int usb_storage_read_sectors(usb_storage_dev_t *sd, u32 lba, u32 count, u8 *buffer) {
    u8 cdb[10];
    memset(cdb, 0, 10);
    cdb[0] = SCSI_READ_10;
    cdb[2] = (u8)(lba >> 24);
    cdb[3] = (u8)(lba >> 16);
    cdb[4] = (u8)(lba >> 8);
    cdb[5] = (u8)(lba);
    cdb[7] = (u8)(count >> 8);
    cdb[8] = (u8)(count);

    u32 data_len = count * 512;
    return bot_transport(sd, cdb, 10, data_len, buffer, 1);
}

static int usb_storage_write_sectors(usb_storage_dev_t *sd, u32 lba, u32 count, const u8 *buffer) {
    u8 cdb[10];
    memset(cdb, 0, 10);
    cdb[0] = SCSI_WRITE_10;
    cdb[2] = (u8)(lba >> 24);
    cdb[3] = (u8)(lba >> 16);
    cdb[4] = (u8)(lba >> 8);
    cdb[5] = (u8)(lba);
    cdb[7] = (u8)(count >> 8);
    cdb[8] = (u8)(count);

    u32 data_len = count * 512;
    return bot_transport(sd, cdb, 10, data_len, (u8 *)buffer, 0);
}

static int stor_blkdev_read(blkdev_t *dev, u32 lba, u32 count, void *buffer) {
    (void)dev;
    if (!stor_dev.initialized) return -1;
    return usb_storage_read_sectors(&stor_dev, lba, count, (u8 *)buffer);
}

static int stor_blkdev_write(blkdev_t *dev, u32 lba, u32 count, const void *buffer) {
    (void)dev;
    if (!stor_dev.initialized) return -1;
    return usb_storage_write_sectors(&stor_dev, lba, count, (const u8 *)buffer);
}

static const blkdev_ops_t stor_blkdev_ops = {
    .read = stor_blkdev_read,
    .write = stor_blkdev_write,
};

static void usb_storage_probe(usb_device_t *dev) {
    if (stor_dev.initialized) {
        console_printf("[usb_stor] storage already active\n");
        return;
    }

    u8 *cfg = dev->config_raw;
    u8 len = dev->config_length;
    u8 iface_num = 0;
    bool found = false;

    u8 offset = 0;
    while (offset < len) {
        u8 desc_len = cfg[offset];
        u8 desc_type = cfg[offset + 1];
        if (desc_len == 0) break;

        if (desc_type == USB_DESC_INTERFACE) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)&cfg[offset];
            if (iface->bInterfaceClass == USB_CLASS_MASS_STORAGE &&
                iface->bInterfaceSubClass == 6 &&
                iface->bInterfaceProtocol == 0x50) {
                iface_num = iface->bInterfaceNumber;
                found = true;
            }
        } else if (desc_type == USB_DESC_ENDPOINT && found) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&cfg[offset];
            if (ep->bEndpointAddress & USB_ENDPOINT_IN) {
                stor_dev.ep_in = ep->bEndpointAddress;
                stor_dev.ep_max_in = ep->wMaxPacketSize;
            } else {
                stor_dev.ep_out = ep->bEndpointAddress;
                stor_dev.ep_max_out = ep->wMaxPacketSize;
            }
        }
        offset += desc_len;
    }

    if (!found || stor_dev.ep_in == 0 || stor_dev.ep_out == 0) {
        console_printf("[usb_stor] missing endpoints\n");
        return;
    }

    stor_dev.usb_dev = dev;
    stor_dev.tag = 1;

    u8 cdb[6];
    memset(cdb, 0, 6);
    cdb[0] = SCSI_TEST_UNIT_READY;
    for (int retry = 0; retry < 10; retry++) {
        int ret = bot_transport(&stor_dev, cdb, 6, 0, NULL, 0);
        if (ret == 0) break;
        pit_sleep(1);
    }

    u8 inquiry_data[36];
    memset(inquiry_data, 0, 36);
    memset(cdb, 0, 6);
    cdb[0] = SCSI_INQUIRY;
    cdb[4] = 36;
    if (bot_transport(&stor_dev, cdb, 6, 36, inquiry_data, 1) < 0) {
        console_printf("[usb_stor] INQUIRY failed\n");
        return;
    }

    u8 cap_data[8];
    memset(cap_data, 0, 8);
    memset(cdb, 0, 10);
    cdb[0] = SCSI_READ_CAPACITY_10;
    if (bot_transport(&stor_dev, cdb, 10, 8, cap_data, 1) < 0) {
        console_printf("[usb_stor] READ CAPACITY failed\n");
        return;
    }

    stor_dev.total_sectors = ((u32)cap_data[0] << 24) |
                             ((u32)cap_data[1] << 16) |
                             ((u32)cap_data[2] << 8) |
                              cap_data[3];
    stor_dev.total_sectors++;

    stor_dev.initialized = true;

    console_printf("[usb_stor] USB storage: %u MB (%u sectors)\n",
                   stor_dev.total_sectors / 2048, stor_dev.total_sectors);
    console_printf("[usb_stor] vendor=%.8s product=%.16s\n",
                   inquiry_data + 8, inquiry_data + 16);

    for (u32 id = 0; id < BLKDEV_MAX_DEVICES; id++) {
        if (!blkdev_present(id)) {
            blkdev_register(id, stor_dev.total_sectors, &stor_blkdev_ops, &stor_dev);
            stor_dev.blkdev_id = id;
            stor_dev.blkdev_registered = true;
            console_printf("[usb_stor] registered as blkdev %d\n", id);
            break;
        }
    }
}

static const usb_class_driver_t stor_driver = {
    .class = USB_CLASS_MASS_STORAGE,
    .subclass = 6,
    .protocol = 0x50,
    .probe = usb_storage_probe,
};

void usb_storage_init(void) {
    memset(&stor_dev, 0, sizeof(stor_dev));
    usb_class_driver_register(&stor_driver);
    console_printf("[usb_stor] mass storage driver registered\n");
}
