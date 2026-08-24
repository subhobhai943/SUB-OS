#ifndef _DRIVERS_FB_H
#define _DRIVERS_FB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t* address;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
    uint8_t   bpp;
    bool      active;
} fb_info_t;

void fb_init(void);
const fb_info_t* fb_get_info(void);
void fb_set_hardware_lfb(uint32_t* lfb_addr, uint32_t width, uint32_t height, uint8_t bpp);

// Mark the framebuffer live or dormant. Drawing routines no-op while dormant,
// which keeps stray writes out of video memory after a return to text mode.
void fb_set_active(bool active);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_clear(uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg);

#endif // _DRIVERS_FB_H
