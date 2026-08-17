#ifndef _DRIVERS_CANVAS_H
#define _DRIVERS_CANVAS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define COLOR_BLACK       0x00000000
#define COLOR_WHITE       0x00FFFFFF
#define COLOR_RED         0x00FF4444
#define COLOR_GREEN       0x0044FF44
#define COLOR_BLUE        0x004488FF
#define COLOR_YELLOW      0x00FFDD44
#define COLOR_CYAN        0x0044DDFF
#define COLOR_DARK_GRAY   0x00222222
#define COLOR_LIGHT_GRAY  0x00CCCCCC
#define COLOR_NAVY        0x001B2A4A
#define COLOR_TITLE_BAR   0x002D3748
#define COLOR_ACCENT      0x003182CE

void canvas_init(void);
void canvas_clear(uint32_t color);
void canvas_draw_pixel(int x, int y, uint32_t color);
void canvas_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void canvas_draw_rect(int x, int y, int w, int h, uint32_t color, bool filled);
void canvas_draw_circle(int cx, int cy, int r, uint32_t color, bool filled);
void canvas_draw_char(int x, int y, char c, uint32_t color, uint32_t bg_color);
void canvas_draw_string(int x, int y, const char* str, uint32_t color, uint32_t bg_color);
void canvas_render_desktop_mockup(void);
void canvas_present(void);

#endif // _DRIVERS_CANVAS_H
