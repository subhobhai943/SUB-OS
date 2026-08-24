#ifndef _DRIVERS_FBCON_H
#define _DRIVERS_FBCON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Framebuffer console.
//
// Once the VBE adapter is switched into graphics mode there is no way back to
// an 80x25 text console without reprogramming the VGA register file, and the
// text buffer at 0xB8000 is no longer being scanned out. Anything printed after
// that point is invisible on the display even though it still reaches serial.
//
// This renders the console into the framebuffer instead, the way Linux fbcon
// does: a character grid drawn with the shared 8x8 font at double height, with
// enough of a VT100 parser (SGR colours, cursor addressing, erase) for
// full-screen curses-style programs such as the nano editor to work.

// The userland models a fixed 80x25 console -- nano pads its bars with %-80s,
// the shell wraps at TTY_WIDTH -- so fbcon presents exactly that geometry and
// scales the glyph cell to fill whatever resolution the adapter is running.
#define FBCON_COLS 80
#define FBCON_ROWS 25

void fbcon_init(void);

// While active, kernel output is drawn here instead of the VGA text buffer.
void fbcon_enable(bool enable);
bool fbcon_is_active(void);

void fbcon_write(const char* data, size_t len);
void fbcon_putc(char c);
void fbcon_clear(void);

int  fbcon_cols(void);
int  fbcon_rows(void);
void fbcon_set_cursor(int row, int col);
void fbcon_get_cursor(int* row, int* col);

#endif // _DRIVERS_FBCON_H
