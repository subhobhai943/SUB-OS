// SUB-WM: window manager and software compositor for SUB-OS
//
// Windows live in a fixed slot array; z-order is a separate array of ids with
// the topmost window last. Interaction is edge-driven: a press picks a target
// and latches it into a drag or resize gesture, and the gesture then owns the
// pointer until the button comes back up.

#include <gui/gui_wm.h>
#include <gui/gui_icons.h>
#include <drivers/keyboard.h>
#include <lib/string.h>
#include <mm/kmalloc.h>

#define BTN_SIZE 14
#define BTN_GAP  4
#define SNAP_MARGIN 8

static gui_window_t g_windows[GUI_MAX_WINDOWS];
static int  g_window_order[GUI_MAX_WINDOWS];
static int  g_num_windows = 0;
static int  g_next_win_id = 1;

static int  g_drag_win_id   = -1;
static int  g_resize_win_id = -1;
static bool g_prev_btn_left = false;
static int  g_last_mouse_x  = 0;
static int  g_last_mouse_y  = 0;

static int  g_work_w = GUI_DEFAULT_WIDTH;
static int  g_work_h = GUI_DEFAULT_HEIGHT;

// Anchor of the in-flight resize gesture, captured on the press edge.
static int  g_resize_start_w  = 0;
static int  g_resize_start_h  = 0;
static int  g_resize_start_mx = 0;
static int  g_resize_start_my = 0;

void gui_wm_init(void) {
    memset(g_windows, 0, sizeof(g_windows));
    memset(g_window_order, 0, sizeof(g_window_order));
    g_num_windows   = 0;
    g_next_win_id   = 1;
    g_drag_win_id   = -1;
    g_resize_win_id = -1;
    g_prev_btn_left = false;
    g_work_w = gui_gfx_get_width();
    g_work_h = gui_gfx_get_height();
}

void gui_wm_set_workarea(int width, int height) {
    if (width > 0)  g_work_w = width;
    if (height > 0) g_work_h = height;
}

gui_window_t* gui_wm_create_window(const char* title, int x, int y, int w, int h) {
    if (g_num_windows >= GUI_MAX_WINDOWS) return NULL;

    int slot = -1;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id == 0) { slot = i; break; }
    }
    if (slot == -1) return NULL;

    gui_window_t* win = &g_windows[slot];
    memset(win, 0, sizeof(*win));

    win->id = g_next_win_id++;
    strncpy(win->title, title ? title : "Window", sizeof(win->title) - 1);

    win->width  = (w > GUI_MIN_WINDOW_W) ? w : GUI_MIN_WINDOW_W;
    win->height = (h > GUI_MIN_WINDOW_H) ? h : GUI_MIN_WINDOW_H;

    // Keep new windows fully on screen even if the caller asked otherwise.
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + win->width > g_work_w)  x = g_work_w - win->width;
    if (y + win->height > g_work_h) y = g_work_h - win->height;
    win->x = (x > 0) ? x : 0;
    win->y = (y > 0) ? y : 0;

    win->visible   = true;
    win->resizable = true;
    win->active    = true;
    win->snap      = GUI_SNAP_NONE;

    g_window_order[g_num_windows++] = win->id;
    gui_wm_focus_window(win->id);
    return win;
}

static int order_index_of(int win_id) {
    for (int i = 0; i < g_num_windows; i++) {
        if (g_window_order[i] == win_id) return i;
    }
    return -1;
}

void gui_wm_destroy_window(int win_id) {
    int idx = order_index_of(win_id);
    if (idx != -1) {
        for (int i = idx; i < g_num_windows - 1; i++) {
            g_window_order[i] = g_window_order[i + 1];
        }
        g_num_windows--;
    }

    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id != win_id) continue;

        if (g_windows[i].handle_event) {
            gui_event_t ev = { .type = GUI_EVENT_CLOSE };
            g_windows[i].handle_event(&g_windows[i], &ev);
        }
        memset(&g_windows[i], 0, sizeof(gui_window_t));
        break;
    }

    if (g_drag_win_id == win_id)   g_drag_win_id = -1;
    if (g_resize_win_id == win_id) g_resize_win_id = -1;

    if (g_num_windows > 0) gui_wm_focus_window(g_window_order[g_num_windows - 1]);
}

void gui_wm_focus_window(int win_id) {
    int idx = order_index_of(win_id);
    if (idx != -1 && idx != g_num_windows - 1) {
        int target = g_window_order[idx];
        for (int i = idx; i < g_num_windows - 1; i++) {
            g_window_order[i] = g_window_order[i + 1];
        }
        g_window_order[g_num_windows - 1] = target;
    }

    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id == 0) continue;

        bool is_active = (g_windows[i].id == win_id);
        if (g_windows[i].active == is_active) continue;

        g_windows[i].active = is_active;
        if (is_active && g_windows[i].handle_event) {
            gui_event_t ev = { .type = GUI_EVENT_FOCUS };
            g_windows[i].handle_event(&g_windows[i], &ev);
        }
    }
}

gui_window_t* gui_wm_get_window(int win_id) {
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].id == win_id) return &g_windows[i];
    }
    return NULL;
}

gui_window_t* gui_wm_get_window_at_index(int index) {
    if (index < 0 || index >= g_num_windows) return NULL;
    return gui_wm_get_window(g_window_order[index]);
}

gui_window_t* gui_wm_get_active_window(void) {
    if (g_num_windows == 0) return NULL;
    return gui_wm_get_window(g_window_order[g_num_windows - 1]);
}

int gui_wm_get_window_count(void) { return g_num_windows; }

// ---------------------------------------------------------------------------
// Window state transitions
// ---------------------------------------------------------------------------

static void save_geometry(gui_window_t* win) {
    if (win->snap != GUI_SNAP_NONE) return; // Already holding a saved rect
    win->saved_x      = win->x;
    win->saved_y      = win->y;
    win->saved_width  = win->width;
    win->saved_height = win->height;
}

static void emit_resize(gui_window_t* win) {
    if (!win->handle_event) return;
    gui_event_t ev = { .type = GUI_EVENT_RESIZE };
    win->handle_event(win, &ev);
}

void gui_wm_snap_window(int win_id, gui_snap_t snap) {
    gui_window_t* win = gui_wm_get_window(win_id);
    if (!win) return;

    if (snap == GUI_SNAP_NONE) {
        gui_wm_restore_window(win_id);
        return;
    }

    save_geometry(win);
    win->snap = snap;

    switch (snap) {
        case GUI_SNAP_LEFT:
            win->x = 0; win->y = 0;
            win->width = g_work_w / 2; win->height = g_work_h;
            break;
        case GUI_SNAP_RIGHT:
            win->x = g_work_w / 2; win->y = 0;
            win->width = g_work_w - g_work_w / 2; win->height = g_work_h;
            break;
        default: // GUI_SNAP_MAXIMIZED
            win->x = 0; win->y = 0;
            win->width = g_work_w; win->height = g_work_h;
            break;
    }

    emit_resize(win);
}

void gui_wm_restore_window(int win_id) {
    gui_window_t* win = gui_wm_get_window(win_id);
    if (!win) return;

    win->minimized = false;
    win->visible   = true;

    if (win->snap != GUI_SNAP_NONE) {
        win->x      = win->saved_x;
        win->y      = win->saved_y;
        win->width  = win->saved_width;
        win->height = win->saved_height;
        win->snap   = GUI_SNAP_NONE;
        emit_resize(win);
    }
}

void gui_wm_toggle_maximize(int win_id) {
    gui_window_t* win = gui_wm_get_window(win_id);
    if (!win) return;

    if (win->snap == GUI_SNAP_MAXIMIZED) gui_wm_restore_window(win_id);
    else                                 gui_wm_snap_window(win_id, GUI_SNAP_MAXIMIZED);
}

void gui_wm_minimize_window(int win_id) {
    gui_window_t* win = gui_wm_get_window(win_id);
    if (!win) return;

    win->minimized = true;

    // Focus drops to the next window that is still on screen.
    for (int i = g_num_windows - 1; i >= 0; i--) {
        gui_window_t* other = gui_wm_get_window(g_window_order[i]);
        if (other && other->id != win_id && !other->minimized && other->visible) {
            gui_wm_focus_window(other->id);
            return;
        }
    }
    win->active = false;
}

void gui_wm_close_active(void) {
    gui_window_t* active = gui_wm_get_active_window();
    if (active) gui_wm_destroy_window(active->id);
}

void gui_wm_cycle_focus(void) {
    if (g_num_windows < 2) return;

    // Walk down from the top and promote the first visible window below it.
    for (int i = g_num_windows - 2; i >= 0; i--) {
        gui_window_t* win = gui_wm_get_window(g_window_order[i]);
        if (win && win->visible) {
            if (win->minimized) win->minimized = false;
            gui_wm_focus_window(win->id);
            return;
        }
    }
}

// After a bulk layout change nothing may hold focus (every window could have
// been minimized). Promote the topmost visible window so the desktop always
// has a keyboard target.
static void ensure_focus(void) {
    if (gui_wm_get_active_window() && gui_wm_get_active_window()->active) return;

    for (int i = g_num_windows - 1; i >= 0; i--) {
        gui_window_t* win = gui_wm_get_window(g_window_order[i]);
        if (win && win->visible && !win->minimized) {
            gui_wm_focus_window(win->id);
            return;
        }
    }
}

void gui_wm_cascade_windows(void) {
    int step = 26;
    int n = 0;

    for (int i = 0; i < g_num_windows; i++) {
        gui_window_t* win = gui_wm_get_window(g_window_order[i]);
        if (!win || !win->visible) continue;

        win->minimized = false;
        win->snap = GUI_SNAP_NONE;
        win->x = 20 + n * step;
        win->y = 20 + n * step;

        if (win->x + win->width > g_work_w)  win->x = g_work_w - win->width;
        if (win->y + win->height > g_work_h) win->y = g_work_h - win->height;
        if (win->x < 0) win->x = 0;
        if (win->y < 0) win->y = 0;

        emit_resize(win);
        n++;
    }

    ensure_focus();
}

void gui_wm_tile_windows(void) {
    int visible = 0;
    for (int i = 0; i < g_num_windows; i++) {
        gui_window_t* win = gui_wm_get_window(g_window_order[i]);
        if (win && win->visible && !win->minimized) visible++;
    }
    if (visible == 0) return;

    // Lay out on the squarest grid that fits every window.
    int cols = 1;
    while (cols * cols < visible) cols++;
    int rows = (visible + cols - 1) / cols;

    int cell_w = g_work_w / cols;
    int cell_h = g_work_h / rows;
    int n = 0;

    for (int i = 0; i < g_num_windows; i++) {
        gui_window_t* win = gui_wm_get_window(g_window_order[i]);
        if (!win || !win->visible || win->minimized) continue;

        win->snap   = GUI_SNAP_NONE;
        win->x      = (n % cols) * cell_w;
        win->y      = (n / cols) * cell_h;
        win->width  = cell_w - 4;
        win->height = cell_h - 4;

        if (win->width < GUI_MIN_WINDOW_W)  win->width = GUI_MIN_WINDOW_W;
        if (win->height < GUI_MIN_WINDOW_H) win->height = GUI_MIN_WINDOW_H;

        emit_resize(win);
        n++;
    }

    ensure_focus();
}

// ---------------------------------------------------------------------------
// Hit testing and pointer gestures
// ---------------------------------------------------------------------------

// Title bar buttons run right to left: 0 = close, 1 = maximize, 2 = minimize.
static void button_rect(const gui_window_t* win, int which, int* bx, int* by) {
    *by = win->y + (GUI_TITLEBAR_HEIGHT - BTN_SIZE) / 2;
    *bx = win->x + win->width - (BTN_SIZE + BTN_GAP) * (which + 1) - 2;
}

static bool point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool in_resize_grip(const gui_window_t* win, int mx, int my) {
    if (!win->resizable || win->snap != GUI_SNAP_NONE) return false;
    return point_in(mx, my,
                    win->x + win->width - GUI_RESIZE_GRIP,
                    win->y + win->height - GUI_RESIZE_GRIP,
                    GUI_RESIZE_GRIP, GUI_RESIZE_GRIP);
}

static void deliver(gui_window_t* win, gui_event_type_t type, int mx, int my,
                    bool left, bool right) {
    if (!win->handle_event) return;

    gui_event_t ev = {
        .type = type,
        .mouse_x = mx,
        .mouse_y = my,
        .rel_x = mx - win->x,
        .rel_y = my - (win->y + GUI_TITLEBAR_HEIGHT),
        .btn_left = left,
        .btn_right = right
    };
    win->handle_event(win, &ev);
}

// A titlebar press: returns true if a button consumed it.
static bool handle_titlebar_press(gui_window_t* win, int mx, int my) {
    int bx, by;

    button_rect(win, 0, &bx, &by);
    if (point_in(mx, my, bx, by, BTN_SIZE, BTN_SIZE)) {
        gui_wm_destroy_window(win->id);
        return true;
    }

    button_rect(win, 1, &bx, &by);
    if (point_in(mx, my, bx, by, BTN_SIZE, BTN_SIZE)) {
        gui_wm_toggle_maximize(win->id);
        return true;
    }

    button_rect(win, 2, &bx, &by);
    if (point_in(mx, my, bx, by, BTN_SIZE, BTN_SIZE)) {
        gui_wm_minimize_window(win->id);
        return true;
    }

    // Anywhere else on the bar starts a drag. Dragging a snapped window pops
    // it back to its saved size first, centred under the cursor.
    if (win->snap != GUI_SNAP_NONE) {
        int grab_ratio = (win->width > 0) ? ((mx - win->x) * 100) / win->width : 50;
        gui_wm_restore_window(win->id);
        win->x = mx - (win->width * grab_ratio) / 100;
        win->y = my - GUI_TITLEBAR_HEIGHT / 2;
        if (win->y < 0) win->y = 0;
    }

    g_drag_win_id   = win->id;
    win->is_dragging = true;
    win->drag_off_x = mx - win->x;
    win->drag_off_y = my - win->y;
    return true;
}

void gui_wm_sync_button_state(bool btn_left) {
    g_prev_btn_left = btn_left;
}

void gui_wm_handle_mouse(int mx, int my, bool btn_left, bool btn_right) {
    g_last_mouse_x = mx;
    g_last_mouse_y = my;

    // --- An in-flight resize owns the pointer until the button is released --
    if (g_resize_win_id != -1) {
        gui_window_t* win = gui_wm_get_window(g_resize_win_id);
        if (!btn_left || !win) {
            if (win) { win->is_resizing = false; emit_resize(win); }
            g_resize_win_id = -1;
        } else {
            int nw = g_resize_start_w + (mx - g_resize_start_mx);
            int nh = g_resize_start_h + (my - g_resize_start_my);

            if (nw < GUI_MIN_WINDOW_W) nw = GUI_MIN_WINDOW_W;
            if (nh < GUI_MIN_WINDOW_H) nh = GUI_MIN_WINDOW_H;
            if (win->x + nw > g_work_w) nw = g_work_w - win->x;
            if (win->y + nh > g_work_h) nh = g_work_h - win->y;

            win->width  = nw;
            win->height = nh;
            g_prev_btn_left = btn_left;
            return;
        }
    }

    // --- An in-flight drag likewise owns the pointer ------------------------
    if (g_drag_win_id != -1) {
        gui_window_t* win = gui_wm_get_window(g_drag_win_id);
        if (!btn_left || !win) {
            if (win) {
                win->is_dragging = false;

                // Releasing against a screen edge snaps the window there.
                if (mx <= SNAP_MARGIN)                 gui_wm_snap_window(win->id, GUI_SNAP_LEFT);
                else if (mx >= g_work_w - SNAP_MARGIN) gui_wm_snap_window(win->id, GUI_SNAP_RIGHT);
                else if (my <= SNAP_MARGIN)            gui_wm_snap_window(win->id, GUI_SNAP_MAXIMIZED);
            }
            g_drag_win_id = -1;
        } else {
            win->x = mx - win->drag_off_x;
            win->y = my - win->drag_off_y;

            // Keep at least a strip of the title bar reachable.
            if (win->y < 0) win->y = 0;
            if (win->y > g_work_h - GUI_TITLEBAR_HEIGHT) win->y = g_work_h - GUI_TITLEBAR_HEIGHT;
            if (win->x + win->width < 60) win->x = 60 - win->width;
            if (win->x > g_work_w - 60)   win->x = g_work_w - 60;

            g_prev_btn_left = btn_left;
            return;
        }
    }

    // --- Press edge: pick a target from the top of the z-order down ---------
    if (btn_left && !g_prev_btn_left) {
        for (int i = g_num_windows - 1; i >= 0; i--) {
            gui_window_t* win = gui_wm_get_window(g_window_order[i]);
            if (!win || !win->visible || win->minimized) continue;
            if (!point_in(mx, my, win->x, win->y, win->width, win->height)) continue;

            gui_wm_focus_window(win->id);

            if (in_resize_grip(win, mx, my)) {
                g_resize_win_id  = win->id;
                win->is_resizing = true;
                g_resize_start_w  = win->width;
                g_resize_start_h  = win->height;
                g_resize_start_mx = mx;
                g_resize_start_my = my;
                g_prev_btn_left = btn_left;
                return;
            }

            if (my < win->y + GUI_TITLEBAR_HEIGHT) {
                handle_titlebar_press(win, mx, my);
                g_prev_btn_left = btn_left;
                return;
            }

            deliver(win, GUI_EVENT_MOUSE_DOWN, mx, my, btn_left, btn_right);
            g_prev_btn_left = btn_left;
            return;
        }

        g_prev_btn_left = btn_left;
        return;
    }

    // --- Release and move edges go to the focused window --------------------
    gui_window_t* active = gui_wm_get_active_window();
    if (active && !active->minimized) {
        if (!btn_left && g_prev_btn_left) {
            deliver(active, GUI_EVENT_MOUSE_UP, mx, my, btn_left, btn_right);
        } else {
            deliver(active, GUI_EVENT_MOUSE_MOVE, mx, my, btn_left, btn_right);
        }
    }

    g_prev_btn_left = btn_left;
}

void gui_wm_handle_key(uint16_t key) {
    // Function keys drive the window manager itself before the app sees them.
    switch (key) {
        case KEY_F5:  gui_wm_cycle_focus();    return;
        case KEY_F6:  gui_wm_cascade_windows(); return;
        case KEY_F7:  gui_wm_tile_windows();   return;
        case KEY_F8: {
            gui_window_t* a = gui_wm_get_active_window();
            if (a) gui_wm_toggle_maximize(a->id);
            return;
        }
        case KEY_F9: {
            gui_window_t* a = gui_wm_get_active_window();
            if (a) gui_wm_minimize_window(a->id);
            return;
        }
        default: break;
    }

    gui_window_t* active = gui_wm_get_active_window();
    if (active && active->handle_event && !active->minimized) {
        gui_event_t ev = { .type = GUI_EVENT_KEY_DOWN, .key = key };
        active->handle_event(active, &ev);
    }
}

// ---------------------------------------------------------------------------
// Compositing
// ---------------------------------------------------------------------------

static void draw_titlebar_buttons(const gui_window_t* win) {
    int bx, by;

    // Close
    button_rect(win, 0, &bx, &by);
    bool hot = point_in(g_last_mouse_x, g_last_mouse_y, bx, by, BTN_SIZE, BTN_SIZE);
    gui_gfx_fill_rect(bx, by, BTN_SIZE, BTN_SIZE, hot ? 0xFFFF6B6B : GUI_THEME_BTN_CLOSE);
    gui_gfx_draw_rect(bx, by, BTN_SIZE, BTN_SIZE, 0xFF991B1B);
    gui_gfx_draw_line(bx + 4, by + 4, bx + BTN_SIZE - 5, by + BTN_SIZE - 5, GUI_COLOR_WHITE);
    gui_gfx_draw_line(bx + BTN_SIZE - 5, by + 4, bx + 4, by + BTN_SIZE - 5, GUI_COLOR_WHITE);

    // Maximize / restore
    button_rect(win, 1, &bx, &by);
    hot = point_in(g_last_mouse_x, g_last_mouse_y, bx, by, BTN_SIZE, BTN_SIZE);
    gui_gfx_fill_rect(bx, by, BTN_SIZE, BTN_SIZE, hot ? 0xFF34D399 : GUI_THEME_BTN_MAXIMIZE);
    gui_gfx_draw_rect(bx, by, BTN_SIZE, BTN_SIZE, 0xFF047857);
    if (win->snap == GUI_SNAP_MAXIMIZED) {
        // Two offset outlines read as "restore down".
        gui_gfx_draw_rect(bx + 3, by + 5, 6, 5, GUI_COLOR_WHITE);
        gui_gfx_draw_rect(bx + 5, by + 3, 6, 5, GUI_COLOR_WHITE);
    } else {
        gui_gfx_draw_rect(bx + 4, by + 4, BTN_SIZE - 8, BTN_SIZE - 8, GUI_COLOR_WHITE);
    }

    // Minimize
    button_rect(win, 2, &bx, &by);
    hot = point_in(g_last_mouse_x, g_last_mouse_y, bx, by, BTN_SIZE, BTN_SIZE);
    gui_gfx_fill_rect(bx, by, BTN_SIZE, BTN_SIZE, hot ? 0xFFFCD34D : GUI_THEME_BTN_MINIMIZE);
    gui_gfx_draw_rect(bx, by, BTN_SIZE, BTN_SIZE, 0xFFB45309);
    gui_gfx_fill_rect(bx + 4, by + BTN_SIZE - 6, BTN_SIZE - 8, 2, GUI_COLOR_WHITE);
}

static void draw_resize_grip(const gui_window_t* win) {
    if (!win->resizable || win->snap != GUI_SNAP_NONE) return;

    int gx = win->x + win->width - GUI_RESIZE_GRIP;
    int gy = win->y + win->height - GUI_RESIZE_GRIP;
    uint32_t col = win->is_resizing ? GUI_THEME_PRIMARY : GUI_THEME_BORDER;

    // Three diagonal hatches, the conventional grip affordance.
    for (int i = 0; i < 3; i++) {
        int off = 3 + i * 3;
        gui_gfx_draw_line(gx + off, gy + GUI_RESIZE_GRIP - 2,
                          gx + GUI_RESIZE_GRIP - 2, gy + off, col);
    }
}

void gui_wm_render(void) {
    // Bottom-to-top painter's algorithm; the topmost window wins overlaps.
    for (int i = 0; i < g_num_windows; i++) {
        gui_window_t* win = gui_wm_get_window(g_window_order[i]);
        if (!win || !win->visible || win->minimized) continue;

        gui_gfx_draw_shadow(win->x, win->y, win->width, win->height, 6);

        uint32_t border = win->active ? GUI_THEME_PRIMARY : GUI_THEME_BORDER;
        gui_gfx_draw_rect(win->x, win->y, win->width, win->height, border);

        uint32_t top_bg = win->active ? 0xFF1E293B : 0xFF0F172A;
        uint32_t bot_bg = win->active ? 0xFF0F172A : 0xFF0A0E1A;
        gui_gfx_draw_gradient_v(win->x + 1, win->y + 1, win->width - 2,
                                GUI_TITLEBAR_HEIGHT - 1, top_bg, bot_bg);
        gui_gfx_draw_line(win->x, win->y + GUI_TITLEBAR_HEIGHT,
                          win->x + win->width - 1, win->y + GUI_TITLEBAR_HEIGHT, border);

        // Truncate the title so it never runs under the buttons.
        int room = win->width - (BTN_SIZE + BTN_GAP) * 3 - 20;
        int max_chars = room / 8;
        if (max_chars < 1) max_chars = 1;
        if (max_chars > 62) max_chars = 62;

        char shown[64];
        strncpy(shown, win->title, (size_t)max_chars);
        shown[max_chars] = '\0';
        if ((int)strlen(win->title) > max_chars && max_chars > 3) {
            shown[max_chars - 1] = '.';
            shown[max_chars - 2] = '.';
        }

        uint32_t title_fg = win->active ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED;
        gui_gfx_draw_string_16_shadow(win->x + 8, win->y + 4, shown, title_fg, GUI_COLOR_BLACK);

        draw_titlebar_buttons(win);

        gui_gfx_fill_rect(win->x + 1, win->y + GUI_TITLEBAR_HEIGHT + 1,
                          win->width - 2, win->height - GUI_TITLEBAR_HEIGHT - 2,
                          GUI_THEME_BG_SURFACE);

        if (win->paint) win->paint(win);

        draw_resize_grip(win);
    }
}
