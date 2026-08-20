#ifndef _GUI_GFX_H
#define _GUI_GFX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <gui/gui_theme.h>

#define GUI_DEFAULT_WIDTH  800
#define GUI_DEFAULT_HEIGHT 600

typedef struct {
    int x;
    int y;
    int width;
    int height;
} gui_rect_t;

void gui_gfx_init(uint32_t* fb, int width, int height);
int gui_gfx_get_width(void);
int gui_gfx_get_height(void);
uint32_t* gui_gfx_get_backbuffer(void);

void gui_gfx_clear(uint32_t color);
void gui_gfx_draw_pixel(int x, int y, uint32_t color);
void gui_gfx_draw_pixel_blend(int x, int y, uint32_t color, uint8_t alpha);
void gui_gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void gui_gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gui_gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gui_gfx_fill_rect_blend(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void gui_gfx_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color, bool filled);
void gui_gfx_draw_gradient_v(int x, int y, int w, int h, uint32_t top_col, uint32_t bot_col);
void gui_gfx_draw_gradient_h(int x, int y, int w, int h, uint32_t left_col, uint32_t right_col);

// Sharp Typography & Font Rendering
void gui_gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent_bg);
void gui_gfx_draw_char_16(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent_bg);
void gui_gfx_draw_string(int x, int y, const char* str, uint32_t fg);
void gui_gfx_draw_string_16(int x, int y, const char* str, uint32_t fg);
void gui_gfx_draw_string_bg(int x, int y, const char* str, uint32_t fg, uint32_t bg);
void gui_gfx_draw_string_shadow(int x, int y, const char* str, uint32_t fg, uint32_t shadow);
void gui_gfx_draw_string_16_shadow(int x, int y, const char* str, uint32_t fg, uint32_t shadow);
void gui_gfx_draw_shadow(int x, int y, int w, int h, int blur);
void gui_gfx_blit(int dx, int dy, int w, int h, const uint32_t* src_buf);
void gui_gfx_present(void);

#endif // _GUI_GFX_H
