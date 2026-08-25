// High-Performance 2D Graphics Engine & Software Compositor for SUB-OS
#include <gui/gui_gfx.h>
#include <lib/font8x8.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

static uint32_t* g_frontbuffer = NULL;
static uint32_t* g_backbuffer = NULL;
/*
 * Mirror of what the framebuffer currently holds.
 *
 * The front buffer lives in device memory, where a write costs far more than
 * the same write to RAM and a read costs more still. Keeping a RAM copy of the
 * last presented frame lets gui_gfx_present() work out which pixels actually
 * changed without ever reading the framebuffer back, and push only those.
 */
static uint32_t* g_shadow = NULL;
static bool g_shadow_valid = false;
static int g_width = 800;
static int g_height = 600;
static uint64_t g_present_pixels = 0;


void gui_gfx_init(uint32_t* fb, int width, int height) {
    int new_w = (width > 0) ? width : 800;
    int new_h = (height > 0) ? height : 600;

    /* A resolution change invalidates both RAM buffers. */
    if (g_backbuffer && (new_w != g_width || new_h != g_height)) {
        kfree(g_backbuffer);
        g_backbuffer = NULL;
        if (g_shadow) {
            kfree(g_shadow);
            g_shadow = NULL;
        }
    }

    g_frontbuffer = fb;
    g_width = new_w;
    g_height = new_h;

    size_t pixels = (size_t)g_width * (size_t)g_height;
    if (!g_backbuffer) {
        g_backbuffer = (uint32_t*)kzalloc(pixels * sizeof(uint32_t));
    }
    if (!g_shadow) {
        /* Optional: without it present() just falls back to a full copy. */
        g_shadow = (uint32_t*)kzalloc(pixels * sizeof(uint32_t));
    }
    g_shadow_valid = false;
}

void gui_gfx_invalidate(void) {
    g_shadow_valid = false;
}

uint64_t gui_gfx_last_present_pixels(void) {
    return g_present_pixels;
}

int gui_gfx_get_width(void) { return g_width; }
int gui_gfx_get_height(void) { return g_height; }
uint32_t* gui_gfx_get_backbuffer(void) { return g_backbuffer; }

void gui_gfx_clear(uint32_t color) {
    if (!g_backbuffer) return;
    size_t count = g_width * g_height;
    for (size_t i = 0; i < count; i++) {
        g_backbuffer[i] = color;
    }
}

void gui_gfx_draw_pixel(int x, int y, uint32_t color) {
    if (!g_backbuffer || x < 0 || y < 0 || x >= g_width || y >= g_height) return;
    g_backbuffer[y * g_width + x] = color;
}

void gui_gfx_draw_pixel_blend(int x, int y, uint32_t color, uint8_t alpha) {
    if (!g_backbuffer || x < 0 || y < 0 || x >= g_width || y >= g_height) return;
    uint32_t bg = g_backbuffer[y * g_width + x];
    g_backbuffer[y * g_width + x] = gui_color_alpha_blend(bg, color, alpha);
}

void gui_gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        gui_gfx_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void gui_gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    gui_gfx_draw_line(x, y, x + w - 1, y, color);
    gui_gfx_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    gui_gfx_draw_line(x, y, x, y + h - 1, color);
    gui_gfx_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void gui_gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_backbuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_width) w = g_width - x;
    if (y + h > g_height) h = g_height - y;
    if (w <= 0 || h <= 0) return;

    for (int j = y; j < y + h; j++) {
        uint32_t* row = &g_backbuffer[j * g_width + x];
        for (int i = 0; i < w; i++) {
            row[i] = color;
        }
    }
}

void gui_gfx_fill_rect_blend(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    if (!g_backbuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_width) w = g_width - x;
    if (y + h > g_height) h = g_height - y;
    if (w <= 0 || h <= 0) return;

    for (int j = y; j < y + h; j++) {
        uint32_t* row = &g_backbuffer[j * g_width + x];
        for (int i = 0; i < w; i++) {
            row[i] = gui_color_alpha_blend(row[i], color, alpha);
        }
    }
}

void gui_gfx_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color, bool filled) {
    if (r <= 0) {
        if (filled) gui_gfx_fill_rect(x, y, w, h, color);
        else gui_gfx_draw_rect(x, y, w, h, color);
        return;
    }

    if (filled) {
        gui_gfx_fill_rect(x + r, y, w - 2 * r, h, color);
        gui_gfx_fill_rect(x, y + r, r, h - 2 * r, color);
        gui_gfx_fill_rect(x + w - r, y + r, r, h - 2 * r, color);
    } else {
        gui_gfx_draw_line(x + r, y, x + w - r, y, color);
        gui_gfx_draw_line(x + r, y + h - 1, x + w - r, y + h - 1, color);
        gui_gfx_draw_line(x, y + r, x, y + h - r, color);
        gui_gfx_draw_line(x + w - 1, y + r, x + w - 1, y + h - r, color);
    }
}

void gui_gfx_draw_gradient_v(int x, int y, int w, int h, uint32_t top_col, uint32_t bot_col) {
    if (h <= 0 || w <= 0) return;
    for (int j = 0; j < h; j++) {
        uint8_t factor = (uint8_t)((j * 255) / h);
        uint32_t row_col = gui_color_alpha_blend(top_col, bot_col, factor);
        gui_gfx_fill_rect(x, y + j, w, 1, row_col);
    }
}

void gui_gfx_draw_gradient_h(int x, int y, int w, int h, uint32_t left_col, uint32_t right_col) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        uint8_t factor = (uint8_t)((i * 255) / w);
        uint32_t col = gui_color_alpha_blend(left_col, right_col, factor);
        gui_gfx_fill_rect(x + i, y, 1, h, col);
    }
}

void gui_gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent_bg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t* glyph = font8x8_glyph(c);

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                gui_gfx_draw_pixel(x + col, y + row, fg);
            } else if (!transparent_bg) {
                gui_gfx_draw_pixel(x + col, y + row, bg);
            }
        }
    }
}

// Sharp High-DPI 8x16 font renderer with 2x vertical pixel fidelity
void gui_gfx_draw_char_16(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent_bg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t* glyph = font8x8_glyph(c);

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col))) {
                gui_gfx_draw_pixel(x + col, y + row * 2, fg);
                gui_gfx_draw_pixel(x + col, y + row * 2 + 1, fg);
            } else if (!transparent_bg) {
                gui_gfx_draw_pixel(x + col, y + row * 2, bg);
                gui_gfx_draw_pixel(x + col, y + row * 2 + 1, bg);
            }
        }
    }
}

void gui_gfx_draw_string(int x, int y, const char* str, uint32_t fg) {
    if (!str) return;
    int cx = x;
    while (*str) {
        gui_gfx_draw_char(cx, y, *str, fg, 0, true);
        cx += 8;
        str++;
    }
}

void gui_gfx_draw_string_16(int x, int y, const char* str, uint32_t fg) {
    if (!str) return;
    int cx = x;
    while (*str) {
        gui_gfx_draw_char_16(cx, y, *str, fg, 0, true);
        cx += 8;
        str++;
    }
}

void gui_gfx_draw_string_bg(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    if (!str) return;
    int cx = x;
    while (*str) {
        gui_gfx_draw_char(cx, y, *str, fg, bg, false);
        cx += 8;
        str++;
    }
}

void gui_gfx_draw_string_shadow(int x, int y, const char* str, uint32_t fg, uint32_t shadow) {
    gui_gfx_draw_string(x + 1, y + 1, str, shadow);
    gui_gfx_draw_string(x, y, str, fg);
}

void gui_gfx_draw_string_16_shadow(int x, int y, const char* str, uint32_t fg, uint32_t shadow) {
    gui_gfx_draw_string_16(x + 1, y + 1, str, shadow);
    gui_gfx_draw_string_16(x, y, str, fg);
}

void gui_gfx_draw_shadow(int x, int y, int w, int h, int blur) {
    for (int r = 1; r <= blur; r++) {
        uint8_t alpha = (uint8_t)(50 / r);
        gui_gfx_fill_rect_blend(x + r, y + h, w, r, GUI_COLOR_BLACK, alpha);
        gui_gfx_fill_rect_blend(x + w, y + r, r, h, GUI_COLOR_BLACK, alpha);
    }
}

void gui_gfx_blit(int dx, int dy, int w, int h, const uint32_t* src_buf) {
    if (!g_backbuffer || !src_buf) return;
    for (int j = 0; j < h; j++) {
        int py = dy + j;
        if (py < 0 || py >= g_height) continue;
        for (int i = 0; i < w; i++) {
            int px = dx + i;
            if (px < 0 || px >= g_width) continue;
            uint32_t pixel = src_buf[j * w + i];
            if ((pixel >> 24) > 0) {
                g_backbuffer[py * g_width + px] = pixel;
            }
        }
    }
}

/*
 * Push the back buffer to the screen, writing only what changed.
 *
 * Every frame is still composited in full, so the cheapest reliable way to
 * find the damage is to diff against the shadow copy: a RAM read is orders of
 * magnitude cheaper than the framebuffer write it avoids. A typical idle
 * desktop frame only moves the cursor and the clock, which turns a 3.6 MB
 * blit at 1280x720 into a few kilobytes.
 *
 * Each row is narrowed to the span between its first and last differing pixel
 * rather than tracked as a set of runs: one memcpy per row keeps the inner
 * loop tight, and a row that changed in two places is rare enough that
 * bridging the gap costs less than the extra bookkeeping.
 */
void gui_gfx_present(void) {
    if (!g_frontbuffer || !g_backbuffer) return;

    size_t pixels = (size_t)g_width * (size_t)g_height;

    /* No shadow (allocation failed) or it does not describe the screen yet:
     * push the whole frame and start tracking from there. */
    if (!g_shadow || !g_shadow_valid) {
        memcpy(g_frontbuffer, g_backbuffer, pixels * sizeof(uint32_t));
        if (g_shadow) {
            memcpy(g_shadow, g_backbuffer, pixels * sizeof(uint32_t));
            g_shadow_valid = true;
        }
        g_present_pixels = pixels;
        return;
    }

    size_t pushed = 0;
    for (int y = 0; y < g_height; y++) {
        size_t row = (size_t)y * (size_t)g_width;
        const uint32_t* back = g_backbuffer + row;
        uint32_t* shadow = g_shadow + row;

        int x0 = 0;
        while (x0 < g_width && back[x0] == shadow[x0]) x0++;
        if (x0 == g_width) continue;            /* row is untouched */

        int x1 = g_width - 1;
        while (x1 > x0 && back[x1] == shadow[x1]) x1--;

        size_t span = (size_t)(x1 - x0 + 1);
        memcpy(g_frontbuffer + row + x0, back + x0, span * sizeof(uint32_t));
        memcpy(shadow + x0, back + x0, span * sizeof(uint32_t));
        pushed += span;
    }

    g_present_pixels = pushed;
}
