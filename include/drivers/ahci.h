#ifndef _DRIVERS_AHCI_H
#define _DRIVERS_AHCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AHCI_PCI_CLASS    0x01
#define AHCI_PCI_SUBCLASS 0x06
#define AHCI_PCI_PROGIF   0x01

#define HBA_PORT_IPM_ACTIVE  1
#define HBA_PORT_DET_PRESENT 3

#define AHCI_DEV_NULL   0
#define AHCI_DEV_SATA   1
#define AHCI_DEV_SEMB   2
#define AHCI_DEV_PM     3
#define AHCI_DEV_SATAPI 4

typedef volatile struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed)) hba_port_t;

typedef volatile struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} __attribute__((packed)) hba_mem_t;

typedef struct {
    char     model[40];
    char     serial[20];
    uint64_t total_sectors;
    uint32_t sector_size;
    uint8_t  port_num;
    bool     present;
} ahci_device_info_t;

bool ahci_init(void);
bool ahci_is_detected(void);
const ahci_device_info_t* ahci_get_port_info(uint8_t port);
int  ahci_read_blocks(uint8_t port, uint64_t lba, uint32_t count, void* buffer);
int  ahci_write_blocks(uint8_t port, uint64_t lba, uint32_t count, const void* buffer);

#endif // _DRIVERS_AHCI_H
