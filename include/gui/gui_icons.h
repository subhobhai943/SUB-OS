#ifndef _GUI_ICONS_H
#define _GUI_ICONS_H

#include <stdint.h>
#include <stdbool.h>

// 16x16 palette-indexed icon set. Each glyph stores one nibble per pixel
// indexing a 16-entry ARGB palette, so the whole set costs 128 bytes an icon
// and can be recoloured per theme without touching the bitmaps.

#define GUI_ICON_SIZE 16

typedef enum {
    GUI_ICON_TERMINAL = 0,
    GUI_ICON_MONITOR,
    GUI_ICON_FOLDER,
    GUI_ICON_CALC,
    GUI_ICON_PAINT,
    GUI_ICON_INFO,
    GUI_ICON_SETTINGS,
    GUI_ICON_TASKS,
    GUI_ICON_LOG,
    GUI_ICON_FLASK,
    GUI_ICON_CLOCK,
    GUI_ICON_FILE,
    GUI_ICON_POWER,
    GUI_ICON_WARNING,
    GUI_ICON_COUNT
} gui_icon_id_t;

void gui_icons_init(void);

// Draw at native 16x16, or scaled by an integer factor (2 = 32x32).
void gui_icon_draw(gui_icon_id_t id, int x, int y);
void gui_icon_draw_scaled(gui_icon_id_t id, int x, int y, int scale);
void gui_icon_draw_tinted(gui_icon_id_t id, int x, int y, int scale, uint32_t tint);

const char* gui_icon_name(gui_icon_id_t id);

#endif // _GUI_ICONS_H
