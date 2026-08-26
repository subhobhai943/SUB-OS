#ifndef _GUI_WIDGETS_H
#define _GUI_WIDGETS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <gui/gui_wm.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>

// SUB-WT: immediate-mode widget toolkit for SUB-OS desktop applications.
//
// An app feeds the toolkit whatever event the window manager delivered
// (gui_widget_feed_event) and then simply calls widget functions from inside
// its paint routine. Each call both draws the control and reports what the
// user did with it, so no retained widget tree has to be kept in sync with
// the window's state.
//
//   static void my_paint(gui_window_t* win) {
//       gui_widget_begin(win);
//       if (gui_button(1, 10, 10, 90, 24, "Refresh")) do_refresh();
//       gui_checkbox(2, 10, 44, "Verbose", &verbose);
//       gui_widget_end();
//   }

#define GUI_FONT_W        8
#define GUI_FONT_H        8
#define GUI_FONT16_H      16
#define GUI_WIDGET_ROW_H  22

typedef enum {
    GUI_ALIGN_LEFT,
    GUI_ALIGN_CENTER,
    GUI_ALIGN_RIGHT
} gui_align_t;

// Per-frame input snapshot the widgets consult.
typedef struct {
    int      mouse_x;        // Window-relative, client area origin
    int      mouse_y;
    bool     mouse_down;     // Button currently held
    bool     mouse_clicked;  // Press edge, consumed by the first widget hit
    bool     mouse_released;
    uint16_t key;            // Pending keystroke, consumed by the focused field
    int      hot_id;         // Widget under the cursor this frame
    int      active_id;      // Widget holding the press
    int      focus_id;       // Widget receiving keystrokes
} gui_widget_input_t;

// Called from an app's handle_event to record what the WM delivered.
void gui_widget_feed_event(gui_window_t* win, const gui_event_t* ev);

// Frame scope. Everything between these two is drawn in client-area
// coordinates: (0,0) is the top-left pixel below the title bar.
void gui_widget_begin(gui_window_t* win);
void gui_widget_end(void);

const gui_widget_input_t* gui_widget_get_input(void);
void gui_widget_set_focus(int id);
int  gui_widget_client_width(void);
int  gui_widget_client_height(void);

// --- Static content ---------------------------------------------------------
void gui_label(int x, int y, const char* text, uint32_t color);
void gui_label_bold(int x, int y, const char* text, uint32_t color);
void gui_label_aligned(int x, int y, int w, const char* text, uint32_t color, gui_align_t align);
void gui_panel(int x, int y, int w, int h, const char* title);
void gui_separator(int x, int y, int w);
void gui_progress_bar(int x, int y, int w, int h, int percent, uint32_t fill);
void gui_sparkline(int x, int y, int w, int h, const uint8_t* samples, int count, uint32_t color);
void gui_badge(int x, int y, const char* text, uint32_t bg, uint32_t fg);

// --- Interactive controls ---------------------------------------------------
// Every control takes a caller-assigned id that must be unique and stable
// within one window.
bool gui_button(int id, int x, int y, int w, int h, const char* label);
bool gui_button_colored(int id, int x, int y, int w, int h, const char* label, uint32_t accent);

// Fully styled variant, for apps that paint their own palette instead of
// inheriting the desktop's slate theme. The three faces cover the idle,
// hovered and pressed states.
bool gui_button_styled(int id, int x, int y, int w, int h, const char* label,
                       uint32_t face, uint32_t face_hover, uint32_t face_press,
                       uint32_t border, uint32_t text);

// Claims a click inside a rectangle without drawing anything, so an app can
// make a custom-drawn region clickable. Place it after the real controls: a
// widget only ever sees a click no earlier widget already claimed.
bool gui_hitzone(int id, int x, int y, int w, int h);
bool gui_checkbox(int id, int x, int y, const char* label, bool* value);
bool gui_radio(int id, int x, int y, const char* label, int* selected, int this_value);
bool gui_slider(int id, int x, int y, int w, int* value, int min_val, int max_val);
bool gui_listbox(int id, int x, int y, int w, int h, const char* const* items, int count, int* selected);
bool gui_textfield(int id, int x, int y, int w, char* buffer, size_t capacity);
int  gui_tabbar(int id, int x, int y, int w, const char* const* labels, int count, int* active);
bool gui_scrollbar(int id, int x, int y, int h, int* offset, int total, int visible);

#endif // _GUI_WIDGETS_H
