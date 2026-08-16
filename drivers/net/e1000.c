#include <drivers/e1000.h>
#include <drivers/pci.h>
#include <net/net.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/io.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define REG_CTRL      0x0000
#define REG_STATUS    0x0008
#define REG_EEPROM    0x0014
#define REG_ICR       0x00C0
#define REG_IMS       0x00D0
#define REG_IMC       0x00D8
#define REG_RCTL      0x0100
#define REG_RDBAL     0x2800
#define REG_RDBAH     0x2804
#define REG_RDLEN     0x2808
#define REG_RDH       0x2810
#define REG_RDT       0x2818
#define REG_TCTL      0x0400
#define REG_TDBAL     0x3800
#define REG_TDBAH     0x3804
#define REG_TDLEN     0x3808
#define REG_TDH       0x3810
#define REG_TDT       0x3818

#define NUM_RX_DESCRIPTORS 32
#define NUM_TX_DESCRIPTORS 16
#define RX_BUFFER_SIZE     2048

typedef struct {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

static pci_device_t* e1000_pci_dev = NULL;
static uint64_t mmio_base = 0;
static uint16_t io_base = 0;
static bool use_mmio = true;
static uint8_t mac_addr[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

static e1000_rx_desc_t* rx_descriptors = NULL;
static uint8_t* rx_buffers[NUM_RX_DESCRIPTORS];
static uint16_t rx_cur = 0;

static e1000_tx_desc_t* tx_descriptors = NULL;
static uint8_t* tx_buffers[NUM_TX_DESCRIPTORS];
static uint16_t tx_cur = 0;

static uint64_t stats_rx_packets = 0;
static uint64_t stats_tx_packets = 0;
static uint64_t stats_rx_bytes = 0;
static uint64_t stats_tx_bytes = 0;

static inline void e1000_write32(uint32_t reg, uint32_t val) {
    if (use_mmio && mmio_base) {
        *((volatile uint32_t*)(mmio_base + reg)) = val;
    } else if (io_base) {
        outl(io_base, reg);
        outl(io_base + 4, val);
    }
}

static inline uint32_t e1000_read32(uint32_t reg) {
    if (use_mmio && mmio_base) {
        return *((volatile uint32_t*)(mmio_base + reg));
    } else if (io_base) {
        outl(io_base, reg);
        return inl(io_base + 4);
    }
    return 0;
}

static void e1000_read_mac(void) {
    uint32_t ral = e1000_read32(0x5400);
    uint32_t rah = e1000_read32(0x5404);

    if (ral != 0 || (rah & 0xFFFF) != 0) {
        mac_addr[0] = ral & 0xFF;
        mac_addr[1] = (ral >> 8) & 0xFF;
        mac_addr[2] = (ral >> 16) & 0xFF;
        mac_addr[3] = (ral >> 24) & 0xFF;
        mac_addr[4] = rah & 0xFF;
        mac_addr[5] = (rah >> 8) & 0xFF;
    }
}

static void e1000_handle_receive(void) {
    while (rx_descriptors[rx_cur].status & 0x01) {
        uint16_t len = rx_descriptors[rx_cur].length;
        uint8_t* buf = rx_buffers[rx_cur];

        stats_rx_packets++;
        stats_rx_bytes += len;

        // Forward to network stack
        net_receive(buf, len);

        rx_descriptors[rx_cur].status = 0;
        uint16_t old_cur = rx_cur;
        rx_cur = (rx_cur + 1) % NUM_RX_DESCRIPTORS;
        e1000_write32(REG_RDT, old_cur);
    }
}

static void e1000_interrupt_handler(registers_t* regs) {
    (void)regs;
    uint32_t icr = e1000_read32(REG_ICR);

    if (icr & 0x80) {
        e1000_handle_receive();
    }
    if (icr & 0x04) {
        printk(KERN_INFO "[E1000] Link status changed: %s\n", e1000_is_link_up() ? "UP" : "DOWN");
    }
}

static void e1000_init_rx(void) {
    rx_descriptors = (e1000_rx_desc_t*)kzalloc(sizeof(e1000_rx_desc_t) * NUM_RX_DESCRIPTORS + 16);
    rx_descriptors = (e1000_rx_desc_t*)(((uint64_t)rx_descriptors + 15) & ~15);

    for (int i = 0; i < NUM_RX_DESCRIPTORS; i++) {
        rx_buffers[i] = (uint8_t*)kmalloc(RX_BUFFER_SIZE + 16);
        rx_descriptors[i].buffer_addr = (uint64_t)rx_buffers[i];
        rx_descriptors[i].status = 0;
    }

    uint64_t rx_desc_phys = (uint64_t)rx_descriptors;
    e1000_write32(REG_RDBAL, (uint32_t)(rx_desc_phys & 0xFFFFFFFF));
    e1000_write32(REG_RDBAH, (uint32_t)(rx_desc_phys >> 32));
    e1000_write32(REG_RDLEN, NUM_RX_DESCRIPTORS * sizeof(e1000_rx_desc_t));
    e1000_write32(REG_RDH, 0);
    e1000_write32(REG_RDT, NUM_RX_DESCRIPTORS - 1);
    rx_cur = 0;

    // RCTL: Enable, Broadcast, Strip CRC, Multicast
    e1000_write32(REG_RCTL, (1 << 1) | (1 << 15) | (1 << 26) | (1 << 4));
}

static void e1000_init_tx(void) {
    tx_descriptors = (e1000_tx_desc_t*)kzalloc(sizeof(e1000_tx_desc_t) * NUM_TX_DESCRIPTORS + 16);
    tx_descriptors = (e1000_tx_desc_t*)(((uint64_t)tx_descriptors + 15) & ~15);

    for (int i = 0; i < NUM_TX_DESCRIPTORS; i++) {
        tx_buffers[i] = (uint8_t*)kmalloc(RX_BUFFER_SIZE + 16);
        tx_descriptors[i].buffer_addr = (uint64_t)tx_buffers[i];
        tx_descriptors[i].status = 1;
        tx_descriptors[i].cmd = 0;
    }

    uint64_t tx_desc_phys = (uint64_t)tx_descriptors;
    e1000_write32(REG_TDBAL, (uint32_t)(tx_desc_phys & 0xFFFFFFFF));
    e1000_write32(REG_TDBAH, (uint32_t)(tx_desc_phys >> 32));
    e1000_write32(REG_TDLEN, NUM_TX_DESCRIPTORS * sizeof(e1000_tx_desc_t));
    e1000_write32(REG_TDH, 0);
    e1000_write32(REG_TDT, 0);
    tx_cur = 0;

    // TCTL: Enable, Pad Short Packets, Collisions
    e1000_write32(REG_TCTL, (1 << 1) | (1 << 3) | (0x0F << 4) | (0x40 << 12));
}

bool e1000_init(void) {
    e1000_pci_dev = pci_find_device(E1000_VENDOR_ID, E1000_DEV_82540EM);
    if (!e1000_pci_dev) e1000_pci_dev = pci_find_device(E1000_VENDOR_ID, E1000_DEV_82545EM);
    if (!e1000_pci_dev) e1000_pci_dev = pci_find_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET);

    if (!e1000_pci_dev) {
        return false;
    }

    pci_enable_bus_mastering(e1000_pci_dev);

    if (e1000_pci_dev->bar_type[0] == 0 && e1000_pci_dev->bar[0] != 0) {
        mmio_base = e1000_pci_dev->bar[0] & ~0xF;
        use_mmio = true;
    } else {
        io_base = (uint16_t)(e1000_pci_dev->bar[1] & ~0x3);
        use_mmio = false;
    }

    e1000_read_mac();

    uint8_t irq = e1000_pci_dev->irq;
    if (irq > 0 && irq <= 15) {
        isr_register_handler(32 + irq, e1000_interrupt_handler);
        pic_clear_mask(irq);
    }

    e1000_init_rx();
    e1000_init_tx();

    e1000_write32(REG_IMS, (1 << 7) | (1 << 4) | (1 << 6) | (1 << 2));
    e1000_read32(REG_ICR);

    printk(KERN_INFO "[E1000] Initialized. MAC: %02x:%02x:%02x:%02x:%02x:%02x (IRQ %d)\n",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], irq);
    return true;
}

void e1000_send(const void* data, uint16_t len) {
    if (!data || len == 0) return;

    while (!(tx_descriptors[tx_cur].status & 0x01)) {
        io_wait();
    }

    memcpy(tx_buffers[tx_cur], data, len);
    tx_descriptors[tx_cur].length = len;
    tx_descriptors[tx_cur].cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP, RS, IFCS
    tx_descriptors[tx_cur].status = 0;

    tx_cur = (tx_cur + 1) % NUM_TX_DESCRIPTORS;
    stats_tx_packets++;
    stats_tx_bytes += len;

    e1000_write32(REG_TDT, tx_cur);
}

void e1000_get_mac(uint8_t* mac_out) {
    if (mac_out) {
        memcpy(mac_out, mac_addr, 6);
    }
}

bool e1000_is_link_up(void) {
    uint32_t status = e1000_read32(REG_STATUS);
    return (status & (1 << 1)) != 0;
}

uint64_t e1000_get_rx_packets(void) { return stats_rx_packets; }
uint64_t e1000_get_tx_packets(void) { return stats_tx_packets; }
uint64_t e1000_get_rx_bytes(void)   { return stats_rx_bytes; }
uint64_t e1000_get_tx_bytes(void)   { return stats_tx_bytes; }
