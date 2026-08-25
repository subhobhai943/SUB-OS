#ifndef _GUI_THEME_H
#define _GUI_THEME_H

#include <stdint.h>

// 32-bit ARGB Color definitions (0xAARRGGBB)
#define GUI_COLOR_TRANSPARENT    0x00000000
#define GUI_COLOR_BLACK          0xFF000000
#define GUI_COLOR_WHITE          0xFFFFFFFF

// Cyberpunk / Nord Dark Theme
#define GUI_THEME_BG_DARK        0xFF0F172A // Slate 900
#define GUI_THEME_BG_SURFACE     0xFF1E293B // Slate 800
#define GUI_THEME_BG_ELEVATED    0xFF334155 // Slate 700
#define GUI_THEME_BORDER         0xFF475569 // Slate 600

#define GUI_THEME_PRIMARY        0xFF38BDF8 // Sky 400
#define GUI_THEME_PRIMARY_DARK   0xFF0284C7 // Sky 600
#define GUI_THEME_ACCENT         0xFF818CF8 // Indigo 400
#define GUI_THEME_SUCCESS        0xFF34D399 // Emerald 400
#define GUI_THEME_WARNING        0xFFFBBF24 // Amber 400
#define GUI_THEME_DANGER         0xFFF87171 // Red 400

#define GUI_THEME_TEXT_MAIN      0xFFF8FAFC // Slate 50
#define GUI_THEME_TEXT_MUTED     0xFF94A3B8 // Slate 400
#define GUI_THEME_TEXT_DIM       0xFF64748B // Slate 500

#define GUI_THEME_TITLEBAR_ACT   0xFF1E293B
#define GUI_THEME_TITLEBAR_INACT 0xFF0F172A
#define GUI_THEME_TASKBAR_BG     0xFF0B0F19

#define GUI_THEME_BTN_CLOSE      0xFFEF4444
#define GUI_THEME_BTN_MINIMIZE   0xFFF59E0B
#define GUI_THEME_BTN_MAXIMIZE   0xFF10B981

static inline uint32_t gui_color_alpha_blend(uint32_t bg, uint32_t fg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

    uint32_t rb_bg = bg & 0x00FF00FF;
    uint32_t g_bg  = bg & 0x0000FF00;
    uint32_t rb_fg = fg & 0x00FF00FF;
    uint32_t g_fg  = fg & 0x0000FF00;

    uint32_t rb = rb_bg + (((rb_fg - rb_bg) * alpha) >> 8);
    uint32_t g  = g_bg  + (((g_fg - g_bg) * alpha) >> 8);

    return 0xFF000000 | (rb & 0x00FF00FF) | (g & 0x0000FF00);
}

#endif // _GUI_THEME_H
