// SUB-WT: Immediate-mode widget toolkit for the SUB-OS desktop
#include <gui/gui_widgets.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <drivers/keyboard.h>
#include <lib/string.h>
#include <lib/printf.h>

static gui_widget_input_t g_input;
static gui_window_t*      g_win = NULL;
static int                g_origin_x = 0;
static int                g_origin_y = 0;
static int                g_client_w = 0;
static int                g_client_h = 0;

// Screen-space helpers: widgets are authored in client coordinates.
static inline int SX(int x) { return g_origin_x + x; }
static inline int SY(int y) { return g_origin_y + y; }

static bool hit_test(int x, int y, int w, int h) {
    return g_input.mouse_x >= x && g_input.mouse_x < x + w &&
           g_input.mouse_y >= y && g_input.mouse_y < y + h;
}

static int text_width(const char* s) {
    return s ? (int)strlen(s) * GUI_FONT_W : 0;
}

void gui_widget_feed_event(gui_window_t* win, const gui_event_t* ev) {
    if (!ev) return;

    switch (ev->type) {
        case GUI_EVENT_MOUSE_DOWN:
            g_input.mouse_x       = ev->rel_x;
            g_input.mouse_y       = ev->rel_y;
            g_input.mouse_down    = true;
            g_input.mouse_clicked = true;
            break;
        case GUI_EVENT_MOUSE_UP:
            g_input.mouse_x        = ev->rel_x;
            g_input.mouse_y        = ev->rel_y;
            g_input.mouse_down     = false;
            g_input.mouse_released = true;
            g_input.active_id      = 0;
            break;
        case GUI_EVENT_MOUSE_MOVE:
            g_input.mouse_x    = ev->rel_x;
            g_input.mouse_y    = ev->rel_y;
            g_input.mouse_down = ev->btn_left;
            break;
        case GUI_EVENT_KEY_DOWN:
            g_input.key = ev->key;
            break;
        case GUI_EVENT_CLOSE:
            g_input.focus_id = 0;
            g_input.active_id = 0;
            break;
        default:
            break;
    }
    (void)win;
}

void gui_widget_begin(gui_window_t* win) {
    g_win = win;
    if (!win) return;

    g_origin_x = win->x + 1;
    g_origin_y = win->y + GUI_TITLEBAR_HEIGHT + 1;
    g_client_w = win->width - 2;
    g_client_h = win->height - GUI_TITLEBAR_HEIGHT - 2;
    g_input.hot_id = 0;
}

void gui_widget_end(void) {
    // One-shot inputs live for exactly one paint pass; anything not claimed
    // by a widget this frame is discarded rather than replayed later.
    g_input.mouse_clicked  = false;
    g_input.mouse_released = false;
    g_input.key            = 0;
    g_win = NULL;
}

const gui_widget_input_t* gui_widget_get_input(void) { return &g_input; }
void gui_widget_set_focus(int id)                    { g_input.focus_id = id; }
int  gui_widget_client_width(void)                   { return g_client_w; }
int  gui_widget_client_height(void)                  { return g_client_h; }

// ===========================================================================
// Static content
// ===========================================================================

void gui_label(int x, int y, const char* text, uint32_t color) {
    gui_gfx_draw_string(SX(x), SY(y), text, color);
}

void gui_label_bold(int x, int y, const char* text, uint32_t color) {
    gui_gfx_draw_string_16_shadow(SX(x), SY(y), text, color, GUI_COLOR_BLACK);
}

void gui_label_aligned(int x, int y, int w, const char* text, uint32_t color, gui_align_t align) {
    int tw = text_width(text);
    int tx = x;
    if (align == GUI_ALIGN_CENTER) tx = x + (w - tw) / 2;
    else if (align == GUI_ALIGN_RIGHT) tx = x + w - tw;
    gui_gfx_draw_string(SX(tx), SY(y), text, color);
}

void gui_panel(int x, int y, int w, int h, const char* title) {
    gui_gfx_fill_rect(SX(x), SY(y), w, h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(SX(x), SY(y), w, h, GUI_THEME_BORDER);

    if (title && *title) {
        // Caption sits astride the top border, so clear the strip behind it.
        int tw = text_width(title) + 8;
        gui_gfx_fill_rect(SX(x + 8), SY(y) - 4, tw, 9, GUI_THEME_BG_SURFACE);
        gui_gfx_draw_string(SX(x + 12), SY(y) - 4, title, GUI_THEME_TEXT_MUTED);
    }
}

void gui_separator(int x, int y, int w) {
    gui_gfx_draw_line(SX(x), SY(y), SX(x + w - 1), SY(y), GUI_THEME_BORDER);
}

void gui_progress_bar(int x, int y, int w, int h, int percent, uint32_t fill) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    gui_gfx_fill_rect(SX(x), SY(y), w, h, GUI_THEME_BG_DARK);
    int filled = (w - 2) * percent / 100;
    if (filled > 0) gui_gfx_fill_rect(SX(x + 1), SY(y + 1), filled, h - 2, fill);
    gui_gfx_draw_rect(SX(x), SY(y), w, h, GUI_THEME_BORDER);
}

void gui_sparkline(int x, int y, int w, int h, const uint8_t* samples, int count, uint32_t color) {
    gui_gfx_fill_rect(SX(x), SY(y), w, h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(SX(x), SY(y), w, h, GUI_THEME_BORDER);
    if (!samples || count < 2) return;

    // Horizontal guides at 25/50/75%.
    for (int q = 1; q < 4; q++) {
        int gy = SY(y) + h * q / 4;
        gui_gfx_draw_line(SX(x) + 1, gy, SX(x) + w - 2, gy, 0xFF1E293B);
    }

    int usable_h = h - 4;
    for (int i = 0; i < count - 1; i++) {
        int x0 = SX(x) + 1 + (i * (w - 2)) / (count - 1);
        int x1 = SX(x) + 1 + ((i + 1) * (w - 2)) / (count - 1);
        int y0 = SY(y) + h - 2 - (samples[i] * usable_h) / 100;
        int y1 = SY(y) + h - 2 - (samples[i + 1] * usable_h) / 100;
        gui_gfx_draw_line(x0, y0, x1, y1, color);
    }
}

void gui_badge(int x, int y, const char* text, uint32_t bg, uint32_t fg) {
    int w = text_width(text) + 10;
    gui_gfx_fill_rect(SX(x), SY(y), w, 14, bg);
    gui_gfx_draw_rect(SX(x), SY(y), w, 14, GUI_THEME_BORDER);
    gui_gfx_draw_string(SX(x + 5), SY(y + 3), text, fg);
}

// ===========================================================================
// Interactive controls
// ===========================================================================

bool gui_hitzone(int id, int x, int y, int w, int h) {
    if (!hit_test(x, y, w, h)) return false;

    g_input.hot_id = id;
    if (!g_input.mouse_clicked) return false;

    g_input.mouse_clicked = false;   // Claim the click
    g_input.active_id     = id;
    return true;
}

bool gui_button_styled(int id, int x, int y, int w, int h, const char* label,
                       uint32_t face, uint32_t face_hover, uint32_t face_press,
                       uint32_t border, uint32_t text) {
    bool hovered = hit_test(x, y, w, h);
    bool pressed = hovered && g_input.mouse_down;
    bool clicked = false;

    if (hovered) {
        g_input.hot_id = id;
        if (g_input.mouse_clicked) {
            g_input.mouse_clicked = false;   // Claim the click
            g_input.active_id     = id;
            g_input.focus_id      = 0;
            clicked = true;
        }
    }

    gui_gfx_fill_rect(SX(x), SY(y), w, h,
                      pressed ? face_press : hovered ? face_hover : face);
    gui_gfx_draw_rect(SX(x), SY(y), w, h, border);

    // A pressed button shifts its label a pixel to sell the depression.
    int tx = x + (w - text_width(label)) / 2 + (pressed ? 1 : 0);
    int ty = y + (h - GUI_FONT_H) / 2 + (pressed ? 1 : 0);
    gui_gfx_draw_string(SX(tx), SY(ty), label, text);

    return clicked;
}

bool gui_button_colored(int id, int x, int y, int w, int h, const char* label, uint32_t accent) {
    bool hovered = hit_test(x, y, w, h);

    return gui_button_styled(id, x, y, w, h, label,
                             GUI_THEME_BG_SURFACE, GUI_THEME_BG_ELEVATED, GUI_THEME_BG_DARK,
                             hovered ? accent : GUI_THEME_BORDER,
                             hovered ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED);
}

bool gui_button(int id, int x, int y, int w, int h, const char* label) {
    return gui_button_colored(id, x, y, w, h, label, GUI_THEME_PRIMARY);
}

bool gui_checkbox(int id, int x, int y, const char* label, bool* value) {
    if (!value) return false;

    int box = 14;
    int w = box + 6 + text_width(label);
    bool hovered = hit_test(x, y, w, box);
    bool changed = false;

    if (hovered && g_input.mouse_clicked) {
        g_input.mouse_clicked = false;
        *value = !*value;
        changed = true;
    }

    gui_gfx_fill_rect(SX(x), SY(y), box, box, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(SX(x), SY(y), box, box, hovered ? GUI_THEME_PRIMARY : GUI_THEME_BORDER);

    if (*value) {
        // Two strokes form the tick.
        gui_gfx_draw_line(SX(x + 3), SY(y + 7), SX(x + 6), SY(y + 10), GUI_THEME_SUCCESS);
        gui_gfx_draw_line(SX(x + 6), SY(y + 10), SX(x + 11), SY(y + 3), GUI_THEME_SUCCESS);
    }

    gui_gfx_draw_string(SX(x + box + 6), SY(y + 3), label,
                        hovered ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED);
    return changed;
}

bool gui_radio(int id, int x, int y, const char* label, int* selected, int this_value) {
    if (!selected) return false;

    int box = 12;
    int w = box + 6 + text_width(label);
    bool hovered = hit_test(x, y, w, box);
    bool changed = false;

    if (hovered && g_input.mouse_clicked) {
        g_input.mouse_clicked = false;
        if (*selected != this_value) {
            *selected = this_value;
            changed = true;
        }
    }

    gui_gfx_draw_rounded_rect(SX(x), SY(y), box, box, 5,
                              hovered ? GUI_THEME_PRIMARY : GUI_THEME_BORDER, false);
    if (*selected == this_value) {
        gui_gfx_fill_rect(SX(x + 4), SY(y + 4), 4, 4, GUI_THEME_PRIMARY);
    }

    gui_gfx_draw_string(SX(x + box + 6), SY(y + 2), label,
                        hovered ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED);
    (void)id;
    return changed;
}

bool gui_slider(int id, int x, int y, int w, int* value, int min_val, int max_val) {
    if (!value || max_val <= min_val) return false;

    int track_h = 6;
    int knob_w  = 10;
    int hit_h   = 18;
    bool hovered = hit_test(x, y - 4, w, hit_h);
    bool changed = false;

    if (hovered && g_input.mouse_clicked) {
        g_input.mouse_clicked = false;
        g_input.active_id = id;
    }

    // Dragging continues while the button is held, even off the track.
    if (g_input.active_id == id && g_input.mouse_down) {
        int rel = g_input.mouse_x - x - knob_w / 2;
        int span = w - knob_w;
        if (span < 1) span = 1;
        if (rel < 0) rel = 0;
        if (rel > span) rel = span;

        int nv = min_val + (rel * (max_val - min_val)) / span;
        if (nv != *value) {
            *value = nv;
            changed = true;
        }
    }

    if (*value < min_val) *value = min_val;
    if (*value > max_val) *value = max_val;

    int track_y = y + (hit_h - track_h) / 2 - 4;
    gui_gfx_fill_rect(SX(x), SY(track_y), w, track_h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(SX(x), SY(track_y), w, track_h, GUI_THEME_BORDER);

    int span = w - knob_w;
    int knob_x = x + ((*value - min_val) * span) / (max_val - min_val);
    gui_gfx_fill_rect(SX(x + 1), SY(track_y + 1), knob_x - x, track_h - 2, GUI_THEME_PRIMARY_DARK);
    gui_gfx_fill_rect(SX(knob_x), SY(y - 3), knob_w, 16,
                      (g_input.active_id == id) ? GUI_THEME_PRIMARY : GUI_THEME_BG_ELEVATED);
    gui_gfx_draw_rect(SX(knob_x), SY(y - 3), knob_w, 16, GUI_THEME_PRIMARY);

    return changed;
}

bool gui_listbox(int id, int x, int y, int w, int h, const char* const* items, int count, int* selected) {
    if (!items || !selected) return false;

    gui_gfx_fill_rect(SX(x), SY(y), w, h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(SX(x), SY(y), w, h, GUI_THEME_BORDER);

    int row_h  = GUI_WIDGET_ROW_H - 4;
    int visible = (h - 4) / row_h;
    bool changed = false;

    // Scroll just enough to keep the selection on screen.
    int first = 0;
    if (*selected >= visible) first = *selected - visible + 1;
    if (first > count - visible) first = count - visible;
    if (first < 0) first = 0;

    for (int i = 0; i < visible && first + i < count; i++) {
        int idx = first + i;
        int ry = y + 2 + i * row_h;
        bool hovered = hit_test(x + 1, ry, w - 2, row_h);

        if (hovered && g_input.mouse_clicked) {
            g_input.mouse_clicked = false;
            g_input.focus_id = id;
            if (*selected != idx) {
                *selected = idx;
                changed = true;
            }
        }

        if (idx == *selected) {
            gui_gfx_fill_rect(SX(x + 1), SY(ry), w - 2, row_h, GUI_THEME_PRIMARY_DARK);
        } else if (hovered) {
            gui_gfx_fill_rect(SX(x + 1), SY(ry), w - 2, row_h, GUI_THEME_BG_ELEVATED);
        }

        uint32_t fg = (idx == *selected) ? GUI_COLOR_WHITE : GUI_THEME_TEXT_MUTED;
        gui_gfx_draw_string(SX(x + 6), SY(ry + (row_h - GUI_FONT_H) / 2), items[idx], fg);
    }

    // Arrow keys move the selection while this list holds focus.
    if (g_input.focus_id == id && g_input.key) {
        if (g_input.key == KEY_UP && *selected > 0) {
            (*selected)--;
            changed = true;
            g_input.key = 0;
        } else if (g_input.key == KEY_DOWN && *selected < count - 1) {
            (*selected)++;
            changed = true;
            g_input.key = 0;
        }
    }

    if (count > visible) {
        gui_gfx_draw_string(SX(x + w - 14), SY(y + 3), "^", GUI_THEME_TEXT_DIM);
        gui_gfx_draw_string(SX(x + w - 14), SY(y + h - 11), "v", GUI_THEME_TEXT_DIM);
    }

    return changed;
}

bool gui_textfield(int id, int x, int y, int w, char* buffer, size_t capacity) {
    if (!buffer || capacity == 0) return false;

    int h = 20;
    bool hovered = hit_test(x, y, w, h);
    bool focused = (g_input.focus_id == id);
    bool changed = false;

    if (hovered && g_input.mouse_clicked) {
        g_input.mouse_clicked = false;
        g_input.focus_id = id;
        focused = true;
    }

    if (focused && g_input.key) {
        uint16_t key = g_input.key;
        char c = (char)(key & 0xFF);
        size_t len = strlen(buffer);

        if (!(key & KEY_SPECIAL_FLAG)) {
            if (c == '\b' || (uint8_t)c == 0x7F) {
                if (len > 0) {
                    buffer[len - 1] = '\0';
                    changed = true;
                }
                g_input.key = 0;
            } else if (c >= 32 && c <= 126 && len + 1 < capacity) {
                buffer[len]     = c;
                buffer[len + 1] = '\0';
                changed = true;
                g_input.key = 0;
            }
        }
    }

    gui_gfx_fill_rect(SX(x), SY(y), w, h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(SX(x), SY(y), w, h,
                      focused ? GUI_THEME_PRIMARY : (hovered ? GUI_THEME_ACCENT : GUI_THEME_BORDER));

    // Show the tail of the text when it overflows the field.
    int max_chars = (w - 10) / GUI_FONT_W;
    int len = (int)strlen(buffer);
    const char* view = (len > max_chars) ? buffer + (len - max_chars) : buffer;

    gui_gfx_draw_string(SX(x + 5), SY(y + (h - GUI_FONT_H) / 2), view, GUI_THEME_TEXT_MAIN);

    if (focused) {
        int cx = x + 5 + (int)strlen(view) * GUI_FONT_W;
        gui_gfx_fill_rect(SX(cx), SY(y + 4), 2, h - 8, GUI_THEME_SUCCESS);
    }

    return changed;
}

int gui_tabbar(int id, int x, int y, int w, const char* const* labels, int count, int* active) {
    if (!labels || count <= 0 || !active) return -1;

    int tab_w = w / count;
    int tab_h = 22;

    for (int i = 0; i < count; i++) {
        int tx = x + i * tab_w;
        bool hovered = hit_test(tx, y, tab_w, tab_h);
        bool is_active = (*active == i);

        if (hovered && g_input.mouse_clicked) {
            g_input.mouse_clicked = false;
            *active = i;
            is_active = true;
        }

        uint32_t bg = is_active ? GUI_THEME_BG_SURFACE
                    : hovered   ? GUI_THEME_BG_ELEVATED
                                : GUI_THEME_BG_DARK;
        gui_gfx_fill_rect(SX(tx), SY(y), tab_w, tab_h, bg);
        gui_gfx_draw_rect(SX(tx), SY(y), tab_w, tab_h, GUI_THEME_BORDER);

        if (is_active) {
            // Active tab gets an accent underline and merges with the body.
            gui_gfx_fill_rect(SX(tx + 1), SY(y), tab_w - 2, 2, GUI_THEME_PRIMARY);
            gui_gfx_draw_line(SX(tx + 1), SY(y + tab_h - 1), SX(tx + tab_w - 2),
                              SY(y + tab_h - 1), GUI_THEME_BG_SURFACE);
        }

        int tw = text_width(labels[i]);
        gui_gfx_draw_string(SX(tx + (tab_w - tw) / 2), SY(y + (tab_h - GUI_FONT_H) / 2),
                            labels[i], is_active ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_DIM);
    }

    (void)id;
    return *active;
}

bool gui_scrollbar(int id, int x, int y, int h, int* offset, int total, int visible) {
    if (!offset || total <= visible || visible <= 0) return false;

    int w = 10;
    gui_gfx_fill_rect(SX(x), SY(y), w, h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(SX(x), SY(y), w, h, GUI_THEME_BORDER);

    int max_offset = total - visible;
    int thumb_h = (h * visible) / total;
    if (thumb_h < 12) thumb_h = 12;

    bool hovered = hit_test(x, y, w, h);
    bool changed = false;

    if (hovered && g_input.mouse_clicked) {
        g_input.mouse_clicked = false;
        g_input.active_id = id;
    }

    if (g_input.active_id == id && g_input.mouse_down) {
        int span = h - thumb_h;
        if (span < 1) span = 1;
        int rel = g_input.mouse_y - y - thumb_h / 2;
        if (rel < 0) rel = 0;
        if (rel > span) rel = span;

        int nv = (rel * max_offset) / span;
        if (nv != *offset) {
            *offset = nv;
            changed = true;
        }
    }

    if (*offset < 0) *offset = 0;
    if (*offset > max_offset) *offset = max_offset;

    int thumb_y = y + ((h - thumb_h) * (*offset)) / (max_offset ? max_offset : 1);
    gui_gfx_fill_rect(SX(x + 1), SY(thumb_y), w - 2, thumb_h,
                      (g_input.active_id == id) ? GUI_THEME_PRIMARY : GUI_THEME_BG_ELEVATED);

    return changed;
}
