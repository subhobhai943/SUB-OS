#ifndef _GUI_WM_H
#define _GUI_WM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>

#define GUI_MAX_WINDOWS      16
#define GUI_TITLEBAR_HEIGHT  24
#define GUI_RESIZE_GRIP      12
#define GUI_MIN_WINDOW_W     160
#define GUI_MIN_WINDOW_H     100

typedef enum {
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_DOWN,
    GUI_EVENT_MOUSE_UP,
    GUI_EVENT_KEY_DOWN,
    GUI_EVENT_FOCUS,
    GUI_EVENT_RESIZE,
    GUI_EVENT_CLOSE
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    int mouse_x;
    int mouse_y;
    int rel_x;
    int rel_y;
    bool btn_left;
    bool btn_right;
    uint16_t key;
} gui_event_t;

// Where a window sits relative to the screen edges.
typedef enum {
    GUI_SNAP_NONE = 0,
    GUI_SNAP_LEFT,
    GUI_SNAP_RIGHT,
    GUI_SNAP_MAXIMIZED
} gui_snap_t;

typedef struct gui_window {
    int  id;
    char title[64];
    int  x;
    int  y;
    int  width;
    int  height;

    bool active;
    bool minimized;
    bool visible;
    bool resizable;

    // Drag / resize interaction state
    bool is_dragging;
    bool is_resizing;
    int  drag_off_x;
    int  drag_off_y;

    // Geometry remembered across maximize / snap so restore is exact
    gui_snap_t snap;
    int  saved_x;
    int  saved_y;
    int  saved_width;
    int  saved_height;

    void* user_data;

    void (*paint)(struct gui_window* win);
    void (*handle_event)(struct gui_window* win, const gui_event_t* ev);
} gui_window_t;

void gui_wm_init(void);

// The work area excludes the taskbar; windows maximize and snap inside it.
void gui_wm_set_workarea(int width, int height);

gui_window_t* gui_wm_create_window(const char* title, int x, int y, int w, int h);
void          gui_wm_destroy_window(int win_id);
void          gui_wm_focus_window(int win_id);
gui_window_t* gui_wm_get_window(int win_id);
gui_window_t* gui_wm_get_active_window(void);
int           gui_wm_get_window_count(void);

// Enumerate in z-order, bottom-most first. Returns NULL past the end.
gui_window_t* gui_wm_get_window_at_index(int index);

void gui_wm_minimize_window(int win_id);
void gui_wm_restore_window(int win_id);
void gui_wm_toggle_maximize(int win_id);
void gui_wm_snap_window(int win_id, gui_snap_t snap);
void gui_wm_close_active(void);
void gui_wm_cycle_focus(void);
void gui_wm_cascade_windows(void);
void gui_wm_tile_windows(void);

// Keep the WM's press-edge detector in step when another layer (a menu, a
// modal dialog) consumed the click instead. Without this the WM still holds
// the pre-press state and reads the still-held button as a fresh press on the
// following frame, stealing focus from whatever the click just opened.
void gui_wm_sync_button_state(bool btn_left);

void gui_wm_handle_mouse(int x, int y, bool btn_left, bool btn_right);
void gui_wm_handle_key(uint16_t key);
void gui_wm_render(void);

#endif // _GUI_WM_H
