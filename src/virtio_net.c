#include "virtio_net.h"
#include "pci.h"
#include "netdev.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "memory.h"
#include "string.h"
#include "console.h"
#include "paging.h"

#define VIRTIO_PCI_VENDOR  0x1AF4
#define VIRTIO_PCI_DEVICE  0x1000

/* Legacy virtio PCI I/O port registers (offset from BAR0) */
#define VIRTIO_PCI_HOST_FEATURES   0x00
#define VIRTIO_PCI_GUEST_FEATURES  0x04
#define VIRTIO_PCI_QUEUE_PFN       0x08
#define VIRTIO_PCI_QUEUE_NUM       0x0C
#define VIRTIO_PCI_QUEUE_SEL       0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY    0x10
#define VIRTIO_PCI_STATUS          0x12
#define VIRTIO_PCI_ISR             0x13
#define VIRTIO_PCI_DEVICE_CFG      0x14

/* Virtio device status */
#define VIRTIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4
#define VIRTIO_STATUS_FEATURES_OK  8
#define VIRTIO_STATUS_FAILED       128

/* Feature bits */
#define VIRTIO_NET_F_MAC          (1 << 5)
#define VIRTIO_NET_F_STATUS       (1 << 16)

/* Virtqueue descriptor flags */
#define VRING_DESC_F_NEXT   1
#define VRING_DESC_F_WRITE  2

/* Virtio-net header prepended to every packet (10 bytes, no num_buffers) */
struct virtio_net_hdr {
    u8 flags;
    u8 gso_type;
    u16 hdr_len;
    u16 gso_size;
    u16 csum_start;
    u16 csum_offset;
} __attribute__((packed));

#define VIRTIO_NET_HDR_SIZE 10
#define VIRTIO_QUEUE_SIZE   64
#define RX_BUF_SIZE         (VIRTIO_NET_HDR_SIZE + 1536)
#define VIRTIO_NUM_RX_BUFS  16

/* VRING descriptor (16 bytes) */
struct vring_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed));

/* Available ring (driver writes) */
struct vring_avail {
    u16 flags;
    u16 idx;
    u16 ring[];
} __attribute__((packed));

/* Used ring element (device writes) */
struct vring_used_elem {
    u32 id;
    u32 len;
} __attribute__((packed));

/* Used ring (device writes) */
struct vring_used {
    u16 flags;
    u16 idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

/* Per-virtqueue state */
struct virtqueue {
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
    u16 queue_index;
    u16 num;
    u32 pfn;
    void *mem;
    u16 free_head;
    u16 last_used_idx;
};

/* Single driver instance (like rtl8139) */
static struct {
    u16 io_base;
    u8 irq;
    u8 mac[6];
    netdev_t *netdev;
    bool initialized;

    struct virtqueue rx_vq;
    struct virtqueue tx_vq;

    /* Posted RX buffers */
    u8 *rx_bufs[VIRTIO_NUM_RX_BUFS];
    u32 rx_bufs_phys[VIRTIO_NUM_RX_BUFS];
    u16 rx_next;

    /* Single TX buffer for synchronous send */
    u8 *tx_buf;
    u32 tx_buf_phys;
    u16 tx_desc_idx;
    bool tx_pending;
} vdev;

/* Round up to 4096 */
#define VIRTQ_ALIGN(n)  (((n) + 4095) & ~4095)

/* Calculate total memory needed for a virtqueue */
static u32 vring_total_size(u16 num)
{
    u32 desc_sz = num * sizeof(struct vring_desc);
    u32 avail_sz = 4 + 2 * num;
    u32 used_off = VIRTQ_ALIGN(desc_sz + avail_sz);
    u32 used_sz = 4 + 8 * num;
    return used_off + used_sz;
}

/* Initialize a virtqueue: allocate memory, program device */
static int virtqueue_setup(struct virtqueue *vq, u16 index, u16 num)
{
    u32 total = vring_total_size(num);

    vq->mem = kmalloc_aligned(total, 4096);
    if (!vq->mem) {
        console_printf("virtio: failed to allocate vq memory (%u bytes)\n", total);
        return -1;
    }
    memset(vq->mem, 0, total);

    vq->num = num;
    vq->queue_index = index;
    vq->free_head = 0;
    vq->last_used_idx = 0;

    u32 desc_sz = num * sizeof(struct vring_desc);
    u32 avail_sz = 4 + 2 * num;
    u32 used_off = VIRTQ_ALIGN(desc_sz + avail_sz);

    vq->desc = (struct vring_desc *)vq->mem;
    vq->avail = (struct vring_avail *)((u8 *)vq->mem + desc_sz);
    vq->used = (struct vring_used *)((u8 *)vq->mem + used_off);
    vq->pfn = (u32)(uintptr_t)vq->mem >> 12;

    /* Select and configure the queue */
    outw(vdev.io_base + VIRTIO_PCI_QUEUE_SEL, index);
    outl(vdev.io_base + VIRTIO_PCI_QUEUE_PFN, vq->pfn);

    console_printf("virtio: vq%u pfn=0x%x num=%u\n", index, vq->pfn, num);
    return 0;
}

/* Post RX buffers to the receive queue */
static void virtio_net_post_rx_buffers(void)
{
    struct virtqueue *vq = &vdev.rx_vq;
    u16 avail_idx = vq->avail->idx;

    for (u16 i = 0; i < VIRTIO_NUM_RX_BUFS; i++) {
        u16 id = (vdev.rx_next + i) % VIRTIO_QUEUE_SIZE;
        vq->desc[id].addr = vdev.rx_bufs_phys[i];
        vq->desc[id].len = RX_BUF_SIZE;
        vq->desc[id].flags = VRING_DESC_F_WRITE;
        vq->avail->ring[(avail_idx + i) % vq->num] = id;
    }

    asm volatile("" ::: "memory");
    vq->avail->idx += VIRTIO_NUM_RX_BUFS;
    asm volatile("" ::: "memory");
    outw(vdev.io_base + VIRTIO_PCI_QUEUE_NOTIFY, vq->queue_index);
}

/** netdev ops: send a packet */
static int virtio_net_netdev_send(netdev_t *dev, const u8 *packet, u16 length)
{
    struct virtqueue *vq = &vdev.tx_vq;
    (void)dev;

    if (length > 1514) return -1;

    /* Wait for previous TX to complete (used ring advances) */
    if (vdev.tx_pending) {
        u16 used_idx = vq->used->idx;
        if (vq->last_used_idx == used_idx)
            return -1;
        vq->last_used_idx = used_idx;
        vdev.tx_pending = false;
    }

    /* Build the packet with virtio-net header */
    memset(vdev.tx_buf, 0, VIRTIO_NET_HDR_SIZE);
    memcpy(vdev.tx_buf + VIRTIO_NET_HDR_SIZE, packet, length);

    /* Set up descriptor */
    u16 id = vdev.tx_desc_idx;
    vq->desc[id].addr = vdev.tx_buf_phys;
    vq->desc[id].len = VIRTIO_NET_HDR_SIZE + length;
    vq->desc[id].flags = 0;

    /* Add to avail ring */
    u16 avail_slot = vq->avail->idx % vq->num;
    vq->avail->ring[avail_slot] = id;

    asm volatile("" ::: "memory");
    vq->avail->idx++;
    vdev.tx_desc_idx = (id + 1) % vq->num;

    asm volatile("" ::: "memory");
    outw(vdev.io_base + VIRTIO_PCI_QUEUE_NOTIFY, vq->queue_index);
    vdev.tx_pending = true;

    return 0;
}

/** netdev ops: enable/disable RX */
static void virtio_net_netdev_rx_enable(netdev_t *dev, bool enable)
{
    (void)dev;
    (void)enable;
}

static const netdev_ops_t virtio_net_ops = {
    .send = virtio_net_netdev_send,
    .rx_enable = virtio_net_netdev_rx_enable,
};

/** IRQ handler */
static void virtio_net_irq_handler(registers_t *regs)
{
    (void)regs;
    u8 isr = inb(vdev.io_base + VIRTIO_PCI_ISR);
    if (!(isr & 1)) return;

    /* Process RX completions */
    struct virtqueue *rx_vq = &vdev.rx_vq;
    u16 rx_used_idx = rx_vq->used->idx;

    while (rx_vq->last_used_idx != rx_used_idx) {
        u16 id = rx_vq->used->ring[rx_vq->last_used_idx % rx_vq->num].id;
        u32 len = rx_vq->used->ring[rx_vq->last_used_idx % rx_vq->num].len;
        rx_vq->last_used_idx++;

        u32 buf_idx = VIRTIO_NUM_RX_BUFS;
        for (u32 i = 0; i < VIRTIO_NUM_RX_BUFS; i++) {
            if (vdev.rx_bufs_phys[i] == (u32)rx_vq->desc[id].addr) {
                buf_idx = i;
                break;
            }
        }
        if (buf_idx >= VIRTIO_NUM_RX_BUFS) continue;

        if (len > VIRTIO_NET_HDR_SIZE) {
            u16 pkt_len = len - VIRTIO_NET_HDR_SIZE;
            if (pkt_len > 1514) pkt_len = 1514;
            netdev_rx_push(vdev.netdev, vdev.rx_bufs[buf_idx] + VIRTIO_NET_HDR_SIZE, pkt_len);
        }

        /* Re-post the buffer */
        rx_vq->desc[id].addr = vdev.rx_bufs_phys[buf_idx];
        rx_vq->desc[id].len = RX_BUF_SIZE;
        rx_vq->desc[id].flags = VRING_DESC_F_WRITE;

        u16 slot = rx_vq->avail->idx % rx_vq->num;
        rx_vq->avail->ring[slot] = id;
        asm volatile("" ::: "memory");
        rx_vq->avail->idx++;
    }

    /* Process TX completions */
    struct virtqueue *tx_vq = &vdev.tx_vq;
    u16 tx_used_idx = tx_vq->used->idx;

    while (tx_vq->last_used_idx != tx_used_idx) {
        tx_vq->last_used_idx++;
        vdev.tx_pending = false;
    }
}

/** Probe and initialize virtio-net device */
static bool virtio_net_probe(const pci_device_t *pci)
{
    vdev.io_base = pci->bar0 & 0xFFFC;
    vdev.irq = pci->irq;

    console_printf("virtio-net: probing at IO=0x%x IRQ=%u\n", vdev.io_base, vdev.irq);

    /* Reset device */
    outb(vdev.io_base + VIRTIO_PCI_STATUS, 0);
    io_wait();

    /* Acknowledge */
    outb(vdev.io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    io_wait();

    /* Set DRIVER */
    outb(vdev.io_base + VIRTIO_PCI_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    io_wait();

    /* Negotiate features */
    u32 host_features = inl(vdev.io_base + VIRTIO_PCI_HOST_FEATURES);

    u32 guest_features = 0;
    if (host_features & VIRTIO_NET_F_MAC)
        guest_features |= VIRTIO_NET_F_MAC;
    if (host_features & VIRTIO_NET_F_STATUS)
        guest_features |= VIRTIO_NET_F_STATUS;

    outl(vdev.io_base + VIRTIO_PCI_GUEST_FEATURES, guest_features);
    io_wait();

    /* Set FEATURES_OK */
    outb(vdev.io_base + VIRTIO_PCI_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    io_wait();

    /* Re-read status to check FEATURES_OK */
    u8 status = inb(vdev.io_base + VIRTIO_PCI_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        console_printf("virtio-net: FEATURES_OK not set\n");
        return false;
    }

    /* Read MAC from device config */
    if (guest_features & VIRTIO_NET_F_MAC) {
        for (int i = 0; i < 6; i++)
            vdev.mac[i] = inb(vdev.io_base + VIRTIO_PCI_DEVICE_CFG + i);
    } else {
        memset(vdev.mac, 0x52, 6);
        vdev.mac[5] = 0x01;
    }

    console_printf("virtio-net: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                   vdev.mac[0], vdev.mac[1], vdev.mac[2],
                   vdev.mac[3], vdev.mac[4], vdev.mac[5]);

    /* Allocate RX buffers */
    for (u32 i = 0; i < VIRTIO_NUM_RX_BUFS; i++) {
        vdev.rx_bufs[i] = kmalloc_aligned(RX_BUF_SIZE, 16);
        if (!vdev.rx_bufs[i]) {
            console_printf("virtio-net: RX buf alloc failed\n");
            return false;
        }
        vdev.rx_bufs_phys[i] = (u32)(uintptr_t)vdev.rx_bufs[i];
    }

    /* Allocate TX buffer (aligned to 16 for DMA safety) */
    vdev.tx_buf = kmalloc_aligned(VIRTIO_NET_HDR_SIZE + 1536, 16);
    if (!vdev.tx_buf) {
        console_printf("virtio-net: TX buf alloc failed\n");
        return false;
    }
    vdev.tx_buf_phys = (u32)(uintptr_t)vdev.tx_buf;

    /* Setup virtqueues */
    if (virtqueue_setup(&vdev.rx_vq, 0, VIRTIO_QUEUE_SIZE) < 0) return false;
    if (virtqueue_setup(&vdev.tx_vq, 1, VIRTIO_QUEUE_SIZE) < 0) return false;

    /* Set DRIVER_OK */
    outb(vdev.io_base + VIRTIO_PCI_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
         VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    io_wait();

    /* Register as netdev */
    vdev.netdev = netdev_register("eth0", vdev.mac, &virtio_net_ops, NULL);
    if (!vdev.netdev) {
        console_printf("virtio-net: netdev_register failed\n");
        return false;
    }

    /* Install IRQ handler */
    irq_install_handler(vdev.irq, virtio_net_irq_handler);
    pic_clear_mask(vdev.irq);

    /* Post initial RX buffers */
    virtio_net_post_rx_buffers();

    console_printf("virtio-net: initialized\n");
    return true;
}

void virtio_net_init(void)
{
    memset(&vdev, 0, sizeof(vdev));

    /* Look for virtio-net on PCI bus 0 */
    u32 count = pci_device_count();
    for (u32 i = 0; i < count; i++) {
        const pci_device_t *dev = pci_device(i);
        if (dev->vendor_id == VIRTIO_PCI_VENDOR &&
            dev->device_id == VIRTIO_PCI_DEVICE &&
            dev->class_code == PCI_CLASS_NETWORK &&
            dev->subclass == PCI_SUBCLASS_ETHERNET) {
            if (virtio_net_probe(dev)) {
                vdev.initialized = true;
                return;
            }
        }
    }

    console_printf("virtio-net: no device found\n");
}
