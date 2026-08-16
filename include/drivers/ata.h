#ifndef _DRIVERS_ATA_H
#define _DRIVERS_ATA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ATA_SECTOR_SIZE 512

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

typedef struct {
    bool present;
    bool is_master;
    uint32_t sector_count;
    char model[41];
    char serial[21];
} ata_device_t;

bool ata_init(void);
bool ata_read_sectors(uint32_t lba, uint8_t count, void* buffer);
bool ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer);
const ata_device_t* ata_get_primary_master(void);

#endif // _DRIVERS_ATA_H
