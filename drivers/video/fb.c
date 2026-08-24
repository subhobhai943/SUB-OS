#include <drivers/fb.h>
#include <lib/font8x8.h>
#include <lib/string.h>
#include <kernel/printk.h>

static fb_info_t fb_info;
static uint32_t fb_mock_buffer[320 * 200];

void fb_init(void) {
    memset(&fb_info, 0, sizeof(fb_info));
    fb_info.width = 320;
    fb_info.height = 200;
    fb_info.pitch = 320 * 4;
    fb_info.bpp = 32;
    fb_info.address = fb_mock_buffer;
    fb_info.active = true;

    printk(KERN_INFO "FB: Linear Framebuffer initialized (%ux%ux%d bpp)\n",
           fb_info.width, fb_info.height, fb_info.bpp);
}

const fb_info_t* fb_get_info(void) {
    return &fb_info;
}

void fb_set_active(bool active) {
    fb_info.active = active;
}

void fb_set_hardware_lfb(uint32_t* lfb_addr, uint32_t width, uint32_t height, uint8_t bpp) {
    if (lfb_addr) {
        fb_info.address = lfb_addr;
        fb_info.width = width;
        fb_info.height = height;
        fb_info.pitch = width * (bpp / 8);
        fb_info.bpp = bpp;
        fb_info.active = true;
    }
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (fb_info.address && x < fb_info.width && y < fb_info.height) {
        fb_info.address[y * fb_info.width + x] = color;
    }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_info.address) return;
    for (uint32_t j = y; j < y + h && j < fb_info.height; j++) {
        for (uint32_t i = x; i < x + w && i < fb_info.width; i++) {
            fb_info.address[j * fb_info.width + i] = color;
        }
    }
}

void fb_clear(uint32_t color) {
    fb_draw_rect(0, 0, fb_info.width, fb_info.height, color);
}

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t* glyph = font8x8_glyph(c);

    // A background of 0 means "leave what is already there", which lets the
    // boot splash draw status text straight over its gradient.
    bool opaque = (bg != 0);

    for (uint32_t row = 0; row < FONT8X8_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT8X8_WIDTH; col++) {
            if (bits & (0x80u >> col)) {
                fb_put_pixel(x + col, y + row, fg);
            } else if (opaque) {
                fb_put_pixel(x + col, y + row, bg);
            }
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg) {
    if (!str) return;
    uint32_t cur_x = x;
    while (*str) {
        fb_draw_char(cur_x, y, *str, fg, bg);
        cur_x += 8;
        str++;
    }
}
