// SUB-OS Window Manager (SUB-WM) Compositor
#include <gui/gui_wm.h>
#include <lib/string.h>
#include <mm/kmalloc.h>

static gui_window_t g_windows[GUI_MAX_WINDOWS];
static int g_window_order[GUI_MAX_WINDOWS]; // Z-order (top window is at index g_num_windows - 1)
static int g_num_windows = 0;
static int g_next_win_id = 1;
static int g_drag_win_id = -1;
static bool g_prev_btn_left = false;

void gui_wm_init(void) {
    memset(g_windows, 0, sizeof(g_windows));
    g_num_windows = 0;
    g_next_win_id = 1;
    g_drag_win_id = -1;
    g_prev_btn_left = false;
}

gui_window_t* gui_wm_create_window(const char* title, int x, int y, int w, int h) {
    if (g_num_windows >= GUI_MAX_WINDOWS) return NULL;

    int slot = -1;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id == 0) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return NULL;

    gui_window_t* win = &g_windows[slot];
    win->id = g_next_win_id++;
    strncpy(win->title, title ? title : "Window", sizeof(win->title) - 1);
    win->x = x;
    win->y = y;
    win->width = (w > 60) ? w : 60;
    win->height = (h > 40) ? h : 40;
    win->visible = true;
    win->minimized = false;
    win->active = true;
    win->is_dragging = false;
    win->user_data = NULL;
    win->paint = NULL;
    win->handle_event = NULL;

    g_window_order[g_num_windows] = win->id;
    g_num_windows++;

    gui_wm_focus_window(win->id);
    return win;
}

void gui_wm_destroy_window(int win_id) {
    int idx = -1;
    for (int i = 0; i < g_num_windows; i++) {
        if (g_window_order[i] == win_id) {
            idx = i;
            break;
        }
    }
    if (idx != -1) {
        for (int i = idx; i < g_num_windows - 1; i++) {
            g_window_order[i] = g_window_order[i + 1];
        }
        g_num_windows--;
    }

    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id == win_id) {
            if (g_windows[i].handle_event) {
                gui_event_t ev = { .type = GUI_EVENT_CLOSE };
                g_windows[i].handle_event(&g_windows[i], &ev);
            }
            memset(&g_windows[i], 0, sizeof(gui_window_t));
            break;
        }
    }

    if (g_num_windows > 0) {
        gui_wm_focus_window(g_window_order[g_num_windows - 1]);
    }
}

void gui_wm_focus_window(int win_id) {
    int idx = -1;
    for (int i = 0; i < g_num_windows; i++) {
        if (g_window_order[i] == win_id) {
            idx = i;
            break;
        }
    }
    if (idx != -1 && idx != g_num_windows - 1) {
        int target = g_window_order[idx];
        for (int i = idx; i < g_num_windows - 1; i++) {
            g_window_order[i] = g_window_order[i + 1];
        }
        g_window_order[g_num_windows - 1] = target;
    }

    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id != 0) {
            bool is_active = (g_windows[i].id == win_id);
            if (g_windows[i].active != is_active) {
                g_windows[i].active = is_active;
                if (is_active && g_windows[i].handle_event) {
                    gui_event_t ev = { .type = GUI_EVENT_FOCUS };
                    g_windows[i].handle_event(&g_windows[i], &ev);
                }
            }
        }
    }
}

gui_window_t* gui_wm_get_window(int win_id) {
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id == win_id) return &g_windows[i];
    }
    return NULL;
}

gui_window_t* gui_wm_get_active_window(void) {
    if (g_num_windows == 0) return NULL;
    int top_id = g_window_order[g_num_windows - 1];
    return gui_wm_get_window(top_id);
}

int gui_wm_get_window_count(void) {
    return g_num_windows;
}

void gui_wm_handle_mouse(int mx, int my, bool btn_left, bool btn_right) {
    // Handle ongoing drag
    if (g_drag_win_id != -1) {
        if (!btn_left) {
            gui_window_t* win = gui_wm_get_window(g_drag_win_id);
            if (win) win->is_dragging = false;
            g_drag_win_id = -1;
        } else {
            gui_window_t* win = gui_wm_get_window(g_drag_win_id);
            if (win) {
                win->x = mx - win->drag_off_x;
                win->y = my - win->drag_off_y;
                if (win->y < 0) win->y = 0;
                if (win->x + win->width < 20) win->x = 20 - win->width;
            }
            g_prev_btn_left = btn_left;
            return;
        }
    }

    // Process top-down clicks
    if (btn_left && !g_prev_btn_left) {
        for (int i = g_num_windows - 1; i >= 0; i--) {
            int win_id = g_window_order[i];
            gui_window_t* win = gui_wm_get_window(win_id);
            if (!win || !win->visible || win->minimized) continue;

            // Check titlebar hit
            if (mx >= win->x && mx < win->x + win->width &&
                my >= win->y && my < win->y + GUI_TITLEBAR_HEIGHT) {
                
                gui_wm_focus_window(win->id);

                // Close button [X] at (win->x + win->width - 14, win->y + 3, 10x10)
                if (mx >= win->x + win->width - 14 && mx <= win->x + win->width - 4) {
                    gui_wm_destroy_window(win->id);
                    g_prev_btn_left = btn_left;
                    return;
                }

                // Titlebar drag start
                g_drag_win_id = win->id;
                win->is_dragging = true;
                win->drag_off_x = mx - win->x;
                win->drag_off_y = my - win->y;
                g_prev_btn_left = btn_left;
                return;
            }

            // Check client area hit
            if (mx >= win->x && mx < win->x + win->width &&
                my >= win->y + GUI_TITLEBAR_HEIGHT && my < win->y + win->height) {
                
                gui_wm_focus_window(win->id);

                if (win->handle_event) {
                    gui_event_t ev = {
                        .type = GUI_EVENT_MOUSE_DOWN,
                        .mouse_x = mx,
                        .mouse_y = my,
                        .rel_x = mx - win->x,
                        .rel_y = my - (win->y + GUI_TITLEBAR_HEIGHT),
                        .btn_left = btn_left,
                        .btn_right = btn_right
                    };
                    win->handle_event(win, &ev);
                }
                g_prev_btn_left = btn_left;
                return;
            }
        }
    } else if (!btn_left && g_prev_btn_left) {
        // Mouse Up event
        gui_window_t* active = gui_wm_get_active_window();
        if (active && active->handle_event) {
            gui_event_t ev = {
                .type = GUI_EVENT_MOUSE_UP,
                .mouse_x = mx,
                .mouse_y = my,
                .rel_x = mx - active->x,
                .rel_y = my - (active->y + GUI_TITLEBAR_HEIGHT),
                .btn_left = btn_left,
                .btn_right = btn_right
            };
            active->handle_event(active, &ev);
        }
    } else {
        // Mouse Move event
        gui_window_t* active = gui_wm_get_active_window();
        if (active && active->handle_event) {
            gui_event_t ev = {
                .type = GUI_EVENT_MOUSE_MOVE,
                .mouse_x = mx,
                .mouse_y = my,
                .rel_x = mx - active->x,
                .rel_y = my - (active->y + GUI_TITLEBAR_HEIGHT),
                .btn_left = btn_left,
                .btn_right = btn_right
            };
            active->handle_event(active, &ev);
        }
    }

    g_prev_btn_left = btn_left;
}

void gui_wm_handle_key(uint16_t key) {
    gui_window_t* active = gui_wm_get_active_window();
    if (active && active->handle_event) {
        gui_event_t ev = {
            .type = GUI_EVENT_KEY_DOWN,
            .key = key
        };
        active->handle_event(active, &ev);
    }
}

void gui_wm_render(void) {
    // Render windows from bottom to top
    for (int i = 0; i < g_num_windows; i++) {
        int win_id = g_window_order[i];
        gui_window_t* win = gui_wm_get_window(win_id);
        if (!win || !win->visible || win->minimized) continue;

        // Window Drop Shadow
        gui_gfx_draw_shadow(win->x, win->y, win->width, win->height, 3);

        // Window Outer Border
        uint32_t border_col = win->active ? GUI_THEME_PRIMARY : GUI_THEME_BORDER;
        gui_gfx_draw_rect(win->x, win->y, win->width, win->height, border_col);

        // Titlebar Background
        uint32_t title_bg = win->active ? GUI_THEME_TITLEBAR_ACT : GUI_THEME_TITLEBAR_INACT;
        gui_gfx_fill_rect(win->x + 1, win->y + 1, win->width - 2, GUI_TITLEBAR_HEIGHT - 1, title_bg);

        // Titlebar Bottom Separator
        gui_gfx_draw_line(win->x, win->y + GUI_TITLEBAR_HEIGHT, win->x + win->width - 1, win->y + GUI_TITLEBAR_HEIGHT, border_col);

        // Titlebar Window Title Text
        uint32_t title_fg = win->active ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED;
        gui_gfx_draw_string_shadow(win->x + 6, win->y + 4, win->title, title_fg, GUI_COLOR_BLACK);

        // Close Button [X]
        int close_x = win->x + win->width - 13;
        int close_y = win->y + 3;
        gui_gfx_fill_rect(close_x, close_y, 10, 10, GUI_THEME_BTN_CLOSE);
        gui_gfx_draw_line(close_x + 2, close_y + 2, close_x + 7, close_y + 7, GUI_COLOR_WHITE);
        gui_gfx_draw_line(close_x + 7, close_y + 2, close_x + 2, close_y + 7, GUI_COLOR_WHITE);

        // Client Area Background
        gui_gfx_fill_rect(win->x + 1, win->y + GUI_TITLEBAR_HEIGHT + 1, win->width - 2, win->height - GUI_TITLEBAR_HEIGHT - 2, GUI_THEME_BG_SURFACE);

        // Custom Window Paint Routine
        if (win->paint) {
            win->paint(win);
        }
    }
}
