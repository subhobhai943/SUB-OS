#ifndef _DRIVERS_TTY_H
#define _DRIVERS_TTY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_TTYS   4
#define TTY_WIDTH  80
#define TTY_HEIGHT 25

void tty_init(void);
void tty_switch(int index);
int  tty_get_current(void);
void tty_write(const char* data, size_t size);
void tty_write_string(const char* data);
void tty_clear(void);
void tty_set_cursor(int row, int col);

#endif // _DRIVERS_TTY_H
