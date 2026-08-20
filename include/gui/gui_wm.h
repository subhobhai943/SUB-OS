#ifndef _GUI_WM_H
#define _GUI_WM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>

#define GUI_MAX_WINDOWS 16
#define GUI_TITLEBAR_HEIGHT 16

typedef enum {
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_DOWN,
    GUI_EVENT_MOUSE_UP,
    GUI_EVENT_KEY_DOWN,
    GUI_EVENT_FOCUS,
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

typedef struct gui_window {
    int id;
    char title[48];
    int x;
    int y;
    int width;
    int height;
    bool active;
    bool minimized;
    bool visible;
    bool is_dragging;
    int drag_off_x;
    int drag_off_y;
    void* user_data;

    void (*paint)(struct gui_window* win);
    void (*handle_event)(struct gui_window* win, const gui_event_t* ev);
} gui_window_t;

void gui_wm_init(void);
gui_window_t* gui_wm_create_window(const char* title, int x, int y, int w, int h);
void gui_wm_destroy_window(int win_id);
void gui_wm_focus_window(int win_id);
gui_window_t* gui_wm_get_window(int win_id);
gui_window_t* gui_wm_get_active_window(void);
int gui_wm_get_window_count(void);

void gui_wm_handle_mouse(int x, int y, bool btn_left, bool btn_right);
void gui_wm_handle_key(uint16_t key);
void gui_wm_render(void);

#endif // _GUI_WM_H
