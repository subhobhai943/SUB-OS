// CMOS / RTC NVRAM driver for SUB-OS.
//
// Reads and writes the MC146818 CMOS register file behind ports 0x70/0x71.
// The NMI-disable bit (0x80) of the index port is preserved on every access so
// this driver never inadvertently re-enables NMIs.

#include <drivers/nvram.h>
#include <kernel/printk.h>

#if defined(__x86_64__)
#include <arch/x86_64/io.h>

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71
#define CMOS_NMI_DISABLE 0x80

uint8_t nvram_read(uint8_t index) {
    uint8_t prev = inb(CMOS_INDEX) & CMOS_NMI_DISABLE;
    outb(CMOS_INDEX, prev | (index & 0x7F));
    return inb(CMOS_DATA);
}

void nvram_write(uint8_t index, uint8_t value) {
    uint8_t prev = inb(CMOS_INDEX) & CMOS_NMI_DISABLE;
    outb(CMOS_INDEX, prev | (index & 0x7F));
    outb(CMOS_DATA, value);
}

uint8_t nvram_checksum(void) {
    uint16_t sum = 0;
    for (int i = NVRAM_USER_START; i < NVRAM_SIZE; i++) {
        sum += nvram_read((uint8_t)i);
    }
    return (uint8_t)(sum & 0xFF);
}

void nvram_dump(void) {
    printk(ANSI_BRIGHT_CYAN "CMOS NVRAM (128 bytes):\n" ANSI_RESET);
    for (int row = 0; row < NVRAM_SIZE; row += 16) {
        printk("  %02X: ", row);
        for (int col = 0; col < 16; col++) {
            printk("%02X ", nvram_read((uint8_t)(row + col)));
        }
        printk("\n");
    }
}

void nvram_init(void) {
    // Byte 0x0F is the BIOS shutdown status; reading it proves the port works.
    uint8_t shutdown_status = nvram_read(0x0F);
    printk(ANSI_BRIGHT_GREEN "NVRAM: " ANSI_RESET
           "CMOS scratch RAM ready (%d bytes, checksum 0x%02X, shutdown-status 0x%02X)\n",
           NVRAM_SIZE, nvram_checksum(), shutdown_status);
}

#else  // CMOS/RTC NVRAM is an x86 platform device.

uint8_t nvram_read(uint8_t index)            { (void)index; return 0; }
void    nvram_write(uint8_t index, uint8_t v){ (void)index; (void)v; }
uint8_t nvram_checksum(void)                 { return 0; }
void    nvram_dump(void)                     { }
void    nvram_init(void)                     { }

#endif
