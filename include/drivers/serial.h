#ifndef _DRIVERS_SERIAL_H
#define _DRIVERS_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define COM1_PORT 0x3F8

void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char* str);
char serial_read_char(void);
bool serial_received(void);

#endif // _DRIVERS_SERIAL_H
