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

// ---------------------------------------------------------------------------
// Save-under overlay: lets the compositor move the cursor without repainting
// the whole scene. The block under the cursor (body + drop shadow) is stashed
// before drawing and restored on the next frame.
// ---------------------------------------------------------------------------
#define CUR_SAVE_W (CURSOR_W + 3)
#define CUR_SAVE_H (CURSOR_H + 3)

static uint32_t g_save[CUR_SAVE_H * CUR_SAVE_W];
static int g_save_x = -1, g_save_y = -1, g_save_w = 0, g_save_h = 0;

// Put back whatever was under the cursor last frame (erasing it).
void gui_cursor_restore_under(void) {
    if (g_save_x < 0) return;
    uint32_t* bb = gui_gfx_get_backbuffer();
    if (!bb) { g_save_x = -1; return; }
    int W = gui_gfx_get_width();
    for (int j = 0; j < g_save_h; j++) {
        uint32_t* dst = &bb[(g_save_y + j) * W + g_save_x];
        const uint32_t* src = &g_save[j * g_save_w];
        for (int i = 0; i < g_save_w; i++) dst[i] = src[i];
    }
    g_save_x = -1;
}

// Stash the block under the current cursor position, then draw the cursor.
void gui_cursor_composite(void) {
    uint32_t* bb = gui_gfx_get_backbuffer();
    if (!bb) return;
    int W = gui_gfx_get_width();
    int H = gui_gfx_get_height();

    int x = g_cur_x, y = g_cur_y;
    int w = CUR_SAVE_W, h = CUR_SAVE_H;
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) { g_save_x = -1; gui_cursor_draw(); return; }

    for (int j = 0; j < h; j++) {
        const uint32_t* srow = &bb[(y + j) * W + x];
        uint32_t* d = &g_save[j * w];
        for (int i = 0; i < w; i++) d[i] = srow[i];
    }
    g_save_x = x; g_save_y = y; g_save_w = w; g_save_h = h;

    gui_cursor_draw();
}
