#ifndef _INIT_BOOTLOGO_H
#define _INIT_BOOTLOGO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// SUB-OS boot logo.
//
// The logo appears in two forms because the framebuffer does not exist for the
// first third of boot. Until the video driver is up, an ASCII rendition goes to
// the text console and the serial line; once fb_init() and bochs_vbe_init()
// have run, the real bitmap takes over as a graphical splash with a progress
// bar, and the remaining boot log continues on serial only.
//
// Both forms are generated from the same artwork by scripts/mklogo.py.

// --- Generated asset (init/logo_data.c) ------------------------------------
extern const int          boot_logo_width;
extern const int          boot_logo_height;
extern const uint32_t     boot_logo_palette[16];
// Bit N set means palette entry N is background and should not be drawn.
extern const uint16_t     boot_logo_transparent_mask;
extern const uint8_t      boot_logo_bitmap[];
extern const char* const  boot_logo_ascii[];

// --- Text console ----------------------------------------------------------
// Draw the ASCII logo and version banner. Safe to call before any driver
// beyond printk is initialised.
void boot_logo_print_text(void);

// --- Graphical splash ------------------------------------------------------
// Take over the framebuffer. Returns false when no framebuffer is available,
// in which case the caller should keep logging to the text console.
bool boot_logo_splash_begin(void);

// Update the splash. `percent` is clamped to 0..100; `status` may be NULL.
void boot_logo_splash_progress(int percent, const char* status);

// Release the splash so the desktop (or the TTY) can own the screen again.
void boot_logo_splash_end(void);

bool boot_logo_splash_active(void);

// Draw the logo at an arbitrary position and integer scale, for callers that
// want the mark without the splash chrome (the desktop About box, say).
void boot_logo_draw(int x, int y, int scale);

#endif // _INIT_BOOTLOGO_H
