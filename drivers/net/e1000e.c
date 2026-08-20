// Intel e1000e PCIe Gigabit Network Adapter Driver (82574L / 82567LM / I217 / I219)
#include <drivers/e1000e.h>
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
#define REG_EECD      0x0010
#define REG_EERD      0x0014
#define REG_ICR       0x00C0
#define REG_ITR       0x00C4
#define REG_ICS       0x00C8
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
#define REG_RAL0      0x5400
#define REG_RAH0      0x5404

#define RCTL_EN       (1 << 1)
#define RCTL_SBP      (1 << 2)
#define RCTL_UPE      (1 << 3)
#define RCTL_MPE      (1 << 4)
#define RCTL_BAM      (1 << 15)
#define RCTL_BSIZE_2K (0 << 16)
#define RCTL_SECRC    (1 << 26)

#define TCTL_EN       (1 << 1)
#define TCTL_PSP      (1 << 3)

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
} __attribute__((packed)) e1000e_rx_desc_t;

typedef struct {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000e_tx_desc_t;

static pci_device_t* e1000e_pci_dev = NULL;
static uint64_t mmio_base = 0;
static bool is_online = false;
static uint8_t mac_addr[6] = {0x52, 0x54, 0x00, 0x82, 0x57, 0x4E};

static e1000e_rx_desc_t* rx_descriptors = NULL;
static uint8_t* rx_buffers[NUM_RX_DESCRIPTORS];
static uint16_t rx_cur = 0;

static e1000e_tx_desc_t* tx_descriptors = NULL;
static uint8_t* tx_buffers[NUM_TX_DESCRIPTORS];
static uint16_t tx_cur = 0;

static uint64_t stats_rx_packets = 0;
static uint64_t stats_tx_packets = 0;
static uint64_t stats_rx_bytes = 0;
static uint64_t stats_tx_bytes = 0;

static inline void write_reg(uint32_t reg, uint32_t val) {
    if (mmio_base) {
        volatile uint32_t* p = (volatile uint32_t*)(mmio_base + reg);
        *p = val;
    }
}

static inline uint32_t read_reg(uint32_t reg) {
    if (mmio_base) {
        volatile uint32_t* p = (volatile uint32_t*)(mmio_base + reg);
        return *p;
    }
    return 0;
}

static uint16_t read_eeprom(uint8_t addr) {
    uint32_t val = (uint32_t)addr << 8 | 1;
    write_reg(REG_EERD, val);
    for (int i = 0; i < 1000; i++) {
        uint32_t status = read_reg(REG_EERD);
        if (status & (1 << 4)) {
            return (uint16_t)((status >> 16) & 0xFFFF);
        }
    }
    return 0;
}

static void e1000e_read_mac(void) {
    uint16_t val = read_eeprom(0);
    if (val != 0 && val != 0xFFFF) {
        mac_addr[0] = val & 0xFF;
        mac_addr[1] = (val >> 8) & 0xFF;
        val = read_eeprom(1);
        mac_addr[2] = val & 0xFF;
        mac_addr[3] = (val >> 8) & 0xFF;
        val = read_eeprom(2);
        mac_addr[4] = val & 0xFF;
        mac_addr[5] = (val >> 8) & 0xFF;
    } else {
        uint32_t ral = read_reg(REG_RAL0);
        uint32_t rah = read_reg(REG_RAH0);
        if (ral != 0) {
            mac_addr[0] = ral & 0xFF;
            mac_addr[1] = (ral >> 8) & 0xFF;
            mac_addr[2] = (ral >> 16) & 0xFF;
            mac_addr[3] = (ral >> 24) & 0xFF;
            mac_addr[4] = rah & 0xFF;
            mac_addr[5] = (rah >> 8) & 0xFF;
        }
    }
}

static void e1000e_rx_init(void) {
    size_t ring_size = sizeof(e1000e_rx_desc_t) * NUM_RX_DESCRIPTORS;
    rx_descriptors = (e1000e_rx_desc_t*)kzalloc(ring_size);

    for (int i = 0; i < NUM_RX_DESCRIPTORS; i++) {
        rx_buffers[i] = (uint8_t*)kmalloc(RX_BUFFER_SIZE);
        rx_descriptors[i].buffer_addr = (uint64_t)rx_buffers[i];
        rx_descriptors[i].status = 0;
    }

    write_reg(REG_RDBAL, (uint32_t)((uint64_t)rx_descriptors & 0xFFFFFFFF));
    write_reg(REG_RDBAH, (uint32_t)(((uint64_t)rx_descriptors >> 32) & 0xFFFFFFFF));
    write_reg(REG_RDLEN, ring_size);
    write_reg(REG_RDH, 0);
    write_reg(REG_RDT, NUM_RX_DESCRIPTORS - 1);
    rx_cur = 0;

    write_reg(REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_BSIZE_2K | RCTL_SECRC);
}

static void e1000e_tx_init(void) {
    size_t ring_size = sizeof(e1000e_tx_desc_t) * NUM_TX_DESCRIPTORS;
    tx_descriptors = (e1000e_tx_desc_t*)kzalloc(ring_size);

    for (int i = 0; i < NUM_TX_DESCRIPTORS; i++) {
        tx_buffers[i] = (uint8_t*)kmalloc(RX_BUFFER_SIZE);
        tx_descriptors[i].buffer_addr = (uint64_t)tx_buffers[i];
        tx_descriptors[i].status = 1; // Descriptor done
        tx_descriptors[i].cmd = 0;
    }

    write_reg(REG_TDBAL, (uint32_t)((uint64_t)tx_descriptors & 0xFFFFFFFF));
    write_reg(REG_TDBAH, (uint32_t)(((uint64_t)tx_descriptors >> 32) & 0xFFFFFFFF));
    write_reg(REG_TDLEN, ring_size);
    write_reg(REG_TDH, 0);
    write_reg(REG_TDT, 0);
    tx_cur = 0;

    write_reg(REG_TCTL, TCTL_EN | TCTL_PSP | (15 << 4) | (64 << 12));
}

void e1000e_init(void) {
    e1000e_pci_dev = pci_find_device(E1000E_VENDOR_INTEL, E1000E_DEV_82574L);
    if (!e1000e_pci_dev) {
        e1000e_pci_dev = pci_find_device(E1000E_VENDOR_INTEL, E1000E_DEV_82567LM);
    }
    if (!e1000e_pci_dev) {
        e1000e_pci_dev = pci_find_device(E1000E_VENDOR_INTEL, E1000E_DEV_I217);
    }
    if (!e1000e_pci_dev) {
        e1000e_pci_dev = pci_find_device(E1000E_VENDOR_INTEL, E1000E_DEV_I219);
    }

    if (!e1000e_pci_dev) {
        // Driver loaded in virtual ready mode
        is_online = false;
        return;
    }

    pci_enable_bus_mastering(e1000e_pci_dev);
    mmio_base = e1000e_pci_dev->bar[0] & ~0xF;

    e1000e_read_mac();
    e1000e_rx_init();
    e1000e_tx_init();

    // Link up & enable interrupts
    write_reg(REG_CTRL, read_reg(REG_CTRL) | (1 << 5) | (1 << 6)); // Auto-speed detection
    write_reg(REG_IMS, 0x1F6DC);

    is_online = true;
    printk(KERN_INFO "E1000E: Intel 82574L PCIe Gigabit NIC online (MAC: %02x:%02x:%02x:%02x:%02x:%02x)\n",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

int e1000e_send(const void* packet, uint16_t length) {
    if (!is_online || !packet || length == 0) return -1;

    e1000e_tx_desc_t* desc = &tx_descriptors[tx_cur];
    if (!(desc->status & 1)) {
        return -1; // Ring full
    }

    memcpy(tx_buffers[tx_cur], packet, length);
    desc->length = length;
    desc->cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP | IFCS | RS
    desc->status = 0;

    uint16_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % NUM_TX_DESCRIPTORS;
    write_reg(REG_TDT, tx_cur);

    stats_tx_packets++;
    stats_tx_bytes += length;
    (void)old_cur;
    return 0;
}

int e1000e_poll(void* buffer, uint16_t max_len) {
    if (!is_online || !buffer) return 0;

    e1000e_rx_desc_t* desc = &rx_descriptors[rx_cur];
    if (!(desc->status & 1)) {
        return 0; // No packet ready
    }

    uint16_t len = desc->length;
    if (len > max_len) len = max_len;

    memcpy(buffer, rx_buffers[rx_cur], len);
    desc->status = 0;

    uint16_t old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % NUM_RX_DESCRIPTORS;
    write_reg(REG_RDT, old_cur);

    stats_rx_packets++;
    stats_rx_bytes += len;
    return len;
}

void e1000e_get_mac(uint8_t* out_mac) {
    if (out_mac) memcpy(out_mac, mac_addr, 6);
}

bool e1000e_is_online(void) {
    return is_online;
}

void e1000e_dump_stats(void) {
    printk(ANSI_BRIGHT_CYAN "=== Intel e1000e PCIe Gigabit Network Adapter ===\n" ANSI_RESET);
    printk("  Device Status  : %s\n", is_online ? ANSI_BRIGHT_GREEN "ONLINE (Gigabit Link Active)" ANSI_RESET : ANSI_YELLOW "STANDBY / NO HARDWARE" ANSI_RESET);
    printk("  MAC Address    : %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    printk("  TX Packets     : %llu (%llu bytes)\n", stats_tx_packets, stats_tx_bytes);
    printk("  RX Packets     : %llu (%llu bytes)\n\n", stats_rx_packets, stats_rx_bytes);
}
