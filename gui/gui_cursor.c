// High-Resolution Smooth Mouse Cursor for SUB-OS GUI
#include <gui/gui_cursor.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>

#define CURSOR_W 16
#define CURSOR_H 24

static int g_cur_x = 400;
static int g_cur_y = 300;

// 16x24 Modern Pointer Cursor (0: Transparent, 1: Black Outline, 2: White Fill, 3: Shadow)
static const uint8_t cursor_bitmap[24][16] = {
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,1,1,1,1,1,1,0,0,0},
    {1,2,2,2,1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,1,2,2,1,0,0,0,0,0,0,0},
    {1,2,1,0,0,1,2,2,1,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,1,2,2,1,0,0,0,0,0,0},
    {1,0,0,0,0,0,1,2,2,1,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,2,1,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,2,1,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

void gui_cursor_init(void) {
    g_cur_x = gui_gfx_get_width() / 2;
    g_cur_y = gui_gfx_get_height() / 2;
}

void gui_cursor_set_pos(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= gui_gfx_get_width()) x = gui_gfx_get_width() - 1;
    if (y >= gui_gfx_get_height()) y = gui_gfx_get_height() - 1;
    g_cur_x = x;
    g_cur_y = y;
}

int gui_cursor_get_x(void) { return g_cur_x; }
int gui_cursor_get_y(void) { return g_cur_y; }

void gui_cursor_draw(void) {
    // 1. Draw Subtle Cursor Drop Shadow
    for (int j = 0; j < CURSOR_H; j++) {
        int py = g_cur_y + j + 2;
        for (int i = 0; i < CURSOR_W; i++) {
            int px = g_cur_x + i + 2;
            uint8_t pixel = cursor_bitmap[j][i];
            if (pixel != 0) {
                gui_gfx_draw_pixel_blend(px, py, GUI_COLOR_BLACK, 100);
            }
        }
    }

    // 2. Draw Cursor Body
    for (int j = 0; j < CURSOR_H; j++) {
        int py = g_cur_y + j;
        for (int i = 0; i < CURSOR_W; i++) {
            int px = g_cur_x + i;
            uint8_t pixel = cursor_bitmap[j][i];
            if (pixel == 1) {
                gui_gfx_draw_pixel(px, py, 0xFF0F172A); // Dark Slate Outline
            } else if (pixel == 2) {
                gui_gfx_draw_pixel(px, py, GUI_COLOR_WHITE); // Pure White Interior
            }
        }
    }
}
