#ifndef _DRIVERS_NVRAM_H
#define _DRIVERS_NVRAM_H

// CMOS / RTC NVRAM driver. The MC146818-compatible RTC exposes 128 indexed
// bytes behind ports 0x70 (index) / 0x71 (data); bytes 0x0E..0x7F are the
// battery-backed scratch RAM used by the BIOS for configuration.

#include <stdint.h>
#include <stdbool.h>

#define NVRAM_SIZE       128
#define NVRAM_USER_START 0x0E   // first non-RTC (scratch) byte

void    nvram_init(void);
uint8_t nvram_read(uint8_t index);
void    nvram_write(uint8_t index, uint8_t value);
uint8_t nvram_checksum(void);        // simple sum over the scratch region
void    nvram_dump(void);            // hex dump of all 128 bytes to the console

#endif // _DRIVERS_NVRAM_H
