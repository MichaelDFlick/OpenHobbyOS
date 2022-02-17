#ifndef OHOS_UHCI_H
#define OHOS_UHCI_H

#include "types.h"
#include "usb.h"
#include "pci.h"

#define UHCI_MAX_PORTS 2

#define UHCI_CMD          0x00
#define UHCI_STS          0x02
#define UHCI_INTR         0x04
#define UHCI_FRNUM        0x06
#define UHCI_FLBASEADD    0x08
#define UHCI_SOFMOD       0x0C
#define UHCI_PORTSC1      0x10
#define UHCI_PORTSC2      0x12

#define UHCI_CMD_RS       (1 << 0)
#define UHCI_CMD_HCRESET  (1 << 1)
#define UHCI_CMD_GRESET   (1 << 2)
#define UHCI_CMD_EGSM     (1 << 3)
#define UHCI_CMD_FGR      (1 << 4)
#define UHCI_CMD_SWDBG    (1 << 5)
#define UHCI_CMD_CF       (1 << 6)
#define UHCI_CMD_MAXP     (1 << 7)

#define UHCI_STS_USBINT   (1 << 0)
#define UHCI_STS_ERROR    (1 << 1)
#define UHCI_STS_RD       (1 << 2)
#define UHCI_STS_HSE      (1 << 3)
#define UHCI_STS_HCPE     (1 << 4)
#define UHCI_STS_HCH      (1 << 5)

#define UHCI_PORTSC_CCS      (1 << 0)
#define UHCI_PORTSC_CSC      (1 << 1)
#define UHCI_PORTSC_PE       (1 << 2)
#define UHCI_PORTSC_PED      (1 << 3)
#define UHCI_PORTSC_LSDA     (1 << 8)
#define UHCI_PORTSC_RESET    (1 << 9)
#define UHCI_PORTSC_SUSPEND  (1 << 12)
#define UHCI_PORTSC_PR       (1 << 13)

#define UHCI_PID_SETUP   0x2D
#define UHCI_PID_OUT     0xE1
#define UHCI_PID_IN      0x69

#define UHCI_TD_ACTIVE      (1 << 23)
#define UHCI_TD_STALLED     (1 << 22)
#define UHCI_TD_BUFERR      (1 << 21)
#define UHCI_TD_CRCERR      (1 << 20)
#define UHCI_TD_NAK         (1 << 19)

typedef struct PACKED {
    u32 link;
    u32 status;
    u32 token;
    u32 buffer;
    u32 buffer_hi;
    u32 reserved[3];
} uhci_td_t;

typedef struct PACKED {
    u32 hlp;
    u32 elp;
    u32 reserved[3];
} uhci_qh_t;

typedef struct {
    u16 io_base;
    u8 irq;
    u8 num_ports;
    u32 *frame_list;
    u32 frame_list_phys;
    uhci_qh_t *async_qh;
    u32 async_qh_phys;
    uhci_td_t *async_tds;
    u32 async_td_phys;
    u8 async_td_count;
    bool initialized;
} uhci_dev_t;

void *uhci_init(const pci_device_t *pci);
usb_hcd_ops_t *uhci_get_ops(void);
void  uhci_enum_ports(void *dev);

#endif
