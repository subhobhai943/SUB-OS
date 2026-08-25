#ifndef _DRIVERS_LPT_H
#define _DRIVERS_LPT_H

// IBM PC parallel port (LPT1) printer driver. Centronics handshake over the
// standard I/O ports 0x378 (data) / 0x379 (status) / 0x37A (control).

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void   lpt_init(void);
bool   lpt_present(void);
int    lpt_putc(char c);                 // 0 on success, -1 on timeout/absent
int    lpt_write(const char* buf, size_t len); // bytes written
uint8_t lpt_status(void);                // raw status register

#endif // _DRIVERS_LPT_H
