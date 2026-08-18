#include <drivers/ata.h>
#include <arch/x86_64/io.h>
#include <kernel/printk.h>
#include <lib/string.h>

static ata_device_t primary_master;

static bool ata_wait_ready(uint16_t io_base) {
    // 400ns delay
    for (int i = 0; i < 4; i++) {
        inb(io_base + 7);
    }

    for (uint32_t timeout = 50000; timeout > 0; timeout--) {
        if (!(inb(io_base + 7) & 0x80)) {
            return true; // BSY bit cleared
        }
    }
    return false;
}

static bool ata_wait_drq(uint16_t io_base) {
    for (uint32_t timeout = 50000; timeout > 0; timeout--) {
        uint8_t status = inb(io_base + 7);
        if (status & 0x08) {
            return true; // DRQ bit set
        }
        if (status & 0x01) {
            return false; // ERR bit set
        }
    }
    return false;
}

bool ata_init(void) {
    memset(&primary_master, 0, sizeof(ata_device_t));

    // Select Primary Master (Drive 0, LBA mode)
    outb(ATA_PRIMARY_IO + 6, 0xA0);
    outb(ATA_PRIMARY_IO + 2, 0);
    outb(ATA_PRIMARY_IO + 3, 0);
    outb(ATA_PRIMARY_IO + 4, 0);
    outb(ATA_PRIMARY_IO + 5, 0);
    outb(ATA_PRIMARY_IO + 7, 0xEC); // IDENTIFY Command

    uint8_t status = inb(ATA_PRIMARY_IO + 7);
    if (status == 0 || status == 0xFF) {
        return false; // No drive
    }

    if (!ata_wait_ready(ATA_PRIMARY_IO)) {
        return false;
    }

    uint8_t mid = inb(ATA_PRIMARY_IO + 4);
    uint8_t hi  = inb(ATA_PRIMARY_IO + 5);
    if (mid != 0 || hi != 0) {
        return false; // ATAPI or not ATA
    }

    if (!ata_wait_drq(ATA_PRIMARY_IO)) {
        return false;
    }

    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(ATA_PRIMARY_IO);
    }

    primary_master.present = true;
    primary_master.is_master = true;
    primary_master.sector_count = (uint32_t)identify_data[60] | ((uint32_t)identify_data[61] << 16);

    // Model String (Words 27-46, byte-swapped)
    char* model_ptr = (char*)&identify_data[27];
    for (int i = 0; i < 40; i += 2) {
        primary_master.model[i] = model_ptr[i + 1];
        primary_master.model[i + 1] = model_ptr[i];
    }
    primary_master.model[40] = '\0';

    // Trim trailing spaces in model
    for (int i = 39; i >= 0 && primary_master.model[i] == ' '; i--) {
        primary_master.model[i] = '\0';
    }

    printk(KERN_INFO "ATA: Primary Master Disk: %s (%llu MB, %u sectors)\n",
           primary_master.model,
           ((uint64_t)primary_master.sector_count * 512) / (1024 * 1024),
           primary_master.sector_count);
    return true;
}

bool ata_read_sectors(uint32_t lba, uint8_t count, void* buffer) {
    if (!primary_master.present || !buffer || count == 0) return false;

    if (!ata_wait_ready(ATA_PRIMARY_IO)) return false;

    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F)); // Master, LBA mode
    outb(ATA_PRIMARY_IO + 2, count);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + 7, 0x20); // READ SECTORS command

    uint16_t* ptr = (uint16_t*)buffer;

    for (uint8_t s = 0; s < count; s++) {
        if (!ata_wait_ready(ATA_PRIMARY_IO) || !ata_wait_drq(ATA_PRIMARY_IO)) {
            return false;
        }

        for (int i = 0; i < 256; i++) {
            *ptr++ = inw(ATA_PRIMARY_IO);
        }
    }

    return true;
}

bool ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer) {
    if (!primary_master.present || !buffer || count == 0) return false;

    if (!ata_wait_ready(ATA_PRIMARY_IO)) return false;

    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + 2, count);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + 7, 0x30); // WRITE SECTORS command

    const uint16_t* ptr = (const uint16_t*)buffer;

    for (uint8_t s = 0; s < count; s++) {
        if (!ata_wait_ready(ATA_PRIMARY_IO) || !ata_wait_drq(ATA_PRIMARY_IO)) {
            return false;
        }

        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_IO, *ptr++);
        }

        outb(ATA_PRIMARY_IO + 7, 0xE7); // Cache Flush
        if (!ata_wait_ready(ATA_PRIMARY_IO)) return false;
    }

    return true;
}

const ata_device_t* ata_get_primary_master(void) {
    return primary_master.present ? &primary_master : NULL;
}
