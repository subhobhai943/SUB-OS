// SUB-OS Desktop Shell: wallpaper, icon grid, taskbar, menus and compositor loop
//
// The shell owns the frame loop. Every iteration it samples the input devices,
// routes the result through the modal dialog layer, then the desktop chrome,
// then the window manager, and finally composites the scene back to front:
// wallpaper, icons, windows, taskbar, menus, dialog, cursor.

#include <gui/gui_desktop.h>
#include <gui/gui_gfx.h>
#include <gui/gui_cursor.h>
#include <gui/gui_wm.h>
#include <gui/gui_apps.h>
#include <gui/gui_terminal.h>
#include <gui/gui_apps_ext.h>
#include <gui/gui_widgets.h>
#include <gui/gui_icons.h>
#include <gui/gui_dialog.h>
#include <gui/gui_theme.h>
#include <drivers/fb.h>
#include <drivers/bochs.h>
#include <drivers/fbcon.h>
#include <drivers/tty.h>
#include <drivers/mouse.h>
#include <drivers/keyboard.h>
#include <drivers/rtc.h>
#include <drivers/cpufreq.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <arch/arch.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define START_MENU_W   232
#define START_ITEM_H   26
#define CONTEXT_MENU_W 150
#define CONTEXT_ITEM_H 22

typedef void (*launch_fn_t)(int x, int y, int w, int h);

typedef struct {
    const char*   label;
    gui_icon_id_t icon;
    launch_fn_t   launch;
    int           x, y, w, h;
} desktop_app_t;

// Launchers used by both the icon grid and the start menu.
static const desktop_app_t g_apps[] = {
    { "Terminal",   GUI_ICON_TERMINAL, gui_app_terminal_launch,  30,  40, 560, 340 },
    { "Files",      GUI_ICON_FOLDER,   gui_app_fileman_launch,   80,  70, 380, 240 },
    { "Monitor",    GUI_ICON_MONITOR,  gui_app_sysmon_launch,   420,  40, 340, 240 },
    { "Tasks",      GUI_ICON_TASKS,    gui_app_taskmgr_launch,  180,  60, 380, 300 },
    { "Kernel Log", GUI_ICON_LOG,      gui_app_logviewer_launch, 60,  90, 640, 330 },
    { "KTest",      GUI_ICON_FLASK,    gui_app_ktest_launch,    140,  80, 470, 250 },
    { "Editor",     GUI_ICON_FILE,     gui_app_editor_launch,   120,  50, 500, 330 },
    { "Calculator", GUI_ICON_CALC,     gui_app_calc_launch,     260, 100, 220, 240 },
    { "Paint",      GUI_ICON_PAINT,    gui_app_paint_launch,    200,  60, 360, 260 },
    { "Clock",      GUI_ICON_CLOCK,    gui_app_clock_launch,    460, 120, 280, 260 },
    { "Settings",   GUI_ICON_SETTINGS, gui_app_settings_launch, 160,  60, 400, 320 },
    { "About",      GUI_ICON_INFO,     gui_app_about_launch,    240, 140, 330, 170 },
};
#define APP_COUNT ((int)(sizeof(g_apps) / sizeof(g_apps[0])))

// Which of the above appear on the desktop itself.
static const int g_desktop_icons[] = { 0, 1, 2, 4, 5, 10 };
#define DESKTOP_ICON_COUNT ((int)(sizeof(g_desktop_icons) / sizeof(g_desktop_icons[0])))

static bool g_start_menu_open   = false;
static bool g_context_menu_open = false;
static int  g_context_x = 0, g_context_y = 0;
static bool g_desktop_running   = false;
static bool g_prev_left = false, g_prev_right = false;
static bool g_show_grid = true;
static int  g_selected_icon = -1;
static int  g_hover_menu_item = -1;
static uint64_t g_frames = 0;
static uint8_t  g_cpu_history[48];

// The launcher geometry below is authored against an 800x600 work area.
#define DESIGN_W 800
#define DESIGN_H 568

static void launch_app(int index) {
    if (index < 0 || index >= APP_COUNT) return;

    const desktop_app_t* a = &g_apps[index];
    if (!a->launch) return;

    int sw = gui_gfx_get_width();
    int wh = gui_desktop_workarea_height();

    // Scale the design geometry to the live resolution, but never shrink below
    // it: a window sized for 800x600 is already at its comfortable minimum.
    int x = (a->x * sw) / DESIGN_W;
    int y = (a->y * wh) / DESIGN_H;
    int w = (a->w * sw) / DESIGN_W;
    int h = (a->h * wh) / DESIGN_H;

    if (w < a->w) w = a->w;
    if (h < a->h) h = a->h;

    // Leave the desktop breathing room: a window scaled straight from the
    // design size swallows most of a 1280x720 screen and buries the wallpaper.
    int max_w = (sw * 62) / 100;
    int max_h = (wh * 72) / 100;
    if (w > max_w) w = max_w;
    if (h > max_h) h = max_h;

    // Keep clear of the icon column so launcher labels stay readable.
    int icon_strip = GUI_DESKTOP_ICON_W + 28;
    if (x < icon_strip) x = icon_strip;
    if (x + w > sw - 8) x = sw - 8 - w;
    if (y + h > wh - 8) y = wh - 8 - h;
    if (y < 8) y = 8;

    a->launch(x, y, w, h);
}

int gui_desktop_workarea_height(void) {
    return gui_gfx_get_height() - GUI_TASKBAR_HEIGHT;
}

void gui_desktop_init(void) {
    const fb_info_t* fb = fb_get_info();
    uint32_t* fb_addr = fb ? fb->address : NULL;
    int w = (fb && fb->width > 0) ? fb->width : GUI_DEFAULT_WIDTH;
    int h = (fb && fb->height > 0) ? fb->height : GUI_DEFAULT_HEIGHT;

    gui_gfx_init(fb_addr, w, h);
    gui_icons_init();
    gui_cursor_init();
    gui_dialog_init();
    gui_wm_init();
    gui_wm_set_workarea(w, h - GUI_TASKBAR_HEIGHT);
    mouse_set_bounds(w, h);

    g_start_menu_open   = false;
    g_context_menu_open = false;
    g_selected_icon     = -1;

    // Seed the taskbar CPU trace with a plausible idle curve.
    for (int i = 0; i < 48; i++) {
        g_cpu_history[i] = (uint8_t)(12 + ((i * 7) % 23));
    }
}

// ===========================================================================
// Wallpaper and desktop icons
// ===========================================================================

void gui_desktop_render_background(void) {
    int sw = gui_gfx_get_width();
    int wh = gui_desktop_workarea_height();

    gui_gfx_draw_gradient_v(0, 0, sw, wh, 0xFF0B132B, 0xFF1C2541);

    if (g_show_grid) {
        // Blend rather than stamp a fixed colour: the gradient's lower half is
        // lighter than any single grid tone, so a constant colour disappears
        // there and only shows near the top of the screen.
        for (int y = 0; y < wh; y += 40) {
            gui_gfx_fill_rect_blend(0, y, sw, 1, GUI_COLOR_BLACK, 40);
        }
        for (int x = 0; x < sw; x += 40) {
            gui_gfx_fill_rect_blend(x, 0, 1, wh, GUI_COLOR_BLACK, 40);
        }
    }

    // Branding, offset to the right so it clears the icon column.
    int bx = sw / 2 + 40;
    int by = wh / 2 - 30;
    gui_gfx_draw_string_16_shadow(bx - 60, by, "SUB-OS DESKTOP", 0xFF38BDF8, 0xFF0284C7);
    gui_gfx_draw_string(bx - 76, by + 22, "Modular Monolithic Kernel v0.2.0-LTS", 0xFF64748B);
    gui_gfx_draw_string(bx - 76, by + 36, "F5 cycle  F6 cascade  F7 tile  F8 max", 0xFF475569);
}

void gui_desktop_render_icons(void) {
    for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
        const desktop_app_t* a = &g_apps[g_desktop_icons[i]];
        int ix = 16;
        int iy = 16 + i * GUI_DESKTOP_ICON_H;
        bool selected = (g_selected_icon == i);

        // A translucent tile behind every icon: several glyphs are dark by
        // design and would otherwise vanish into the dark wallpaper.
        if (selected) {
            gui_gfx_fill_rect_blend(ix - 4, iy - 4, GUI_DESKTOP_ICON_W,
                                    GUI_DESKTOP_ICON_H - 4, GUI_THEME_PRIMARY, 70);
            gui_gfx_draw_rect(ix - 4, iy - 4, GUI_DESKTOP_ICON_W,
                              GUI_DESKTOP_ICON_H - 4, GUI_THEME_PRIMARY);
        } else {
            gui_gfx_fill_rect_blend(ix + 10, iy - 4, 40, 40, GUI_THEME_BG_ELEVATED, 150);
            gui_gfx_draw_rect(ix + 10, iy - 4, 40, 40, 0xFF3D4C63);
        }

        gui_icon_draw_scaled(a->icon, ix + 14, iy, 2);

        int tw = (int)strlen(a->label) * 8;
        int tx = ix + 30 - tw / 2;
        if (tx < 0) tx = 0;
        gui_gfx_draw_string_shadow(tx, iy + 40, a->label,
                                   GUI_THEME_TEXT_MAIN, GUI_COLOR_BLACK);
    }
}

// Returns the icon index under the point, or -1.
static int icon_hit_test(int mx, int my) {
    for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
        int ix = 12;
        int iy = 12 + i * GUI_DESKTOP_ICON_H;
        if (mx >= ix && mx < ix + GUI_DESKTOP_ICON_W &&
            my >= iy && my < iy + GUI_DESKTOP_ICON_H - 4) {
            return i;
        }
    }
    return -1;
}

// ===========================================================================
// Taskbar
// ===========================================================================

void gui_desktop_render_taskbar(void) {
    int sw = gui_gfx_get_width();
    int ty = gui_desktop_workarea_height();

    gui_gfx_draw_gradient_v(0, ty, sw, GUI_TASKBAR_HEIGHT, 0xFF111827, GUI_THEME_TASKBAR_BG);
    gui_gfx_draw_line(0, ty, sw - 1, ty, GUI_THEME_BORDER);

    // Start button
    uint32_t start_bg = g_start_menu_open ? GUI_THEME_PRIMARY_DARK : GUI_THEME_PRIMARY;
    gui_gfx_fill_rect(6, ty + 4, 92, 24, start_bg);
    gui_gfx_draw_rect(6, ty + 4, 92, 24, GUI_THEME_PRIMARY_DARK);
    gui_icon_draw(GUI_ICON_TERMINAL, 11, ty + 8);
    gui_gfx_draw_string_16_shadow(32, ty + 8, "SUB-OS", GUI_COLOR_WHITE, GUI_COLOR_BLACK);

    // One button per window, in z-order
    int tab_x = 106;
    int tray_x = sw - 200;
    for (int i = 0; i < gui_wm_get_window_count() && tab_x < tray_x - 106; i++) {
        gui_window_t* win = gui_wm_get_window_at_index(i);
        if (!win || !win->visible) continue;

        uint32_t tab_bg = win->active && !win->minimized ? GUI_THEME_BG_ELEVATED
                                                         : GUI_THEME_BG_SURFACE;
        gui_gfx_fill_rect(tab_x, ty + 4, 100, 24, tab_bg);
        gui_gfx_draw_rect(tab_x, ty + 4, 100, 24,
                          win->active && !win->minimized ? GUI_THEME_PRIMARY : GUI_THEME_BORDER);

        // Active window gets an accent underline; minimized ones are dimmed.
        if (win->active && !win->minimized) {
            gui_gfx_fill_rect(tab_x + 1, ty + 25, 98, 2, GUI_THEME_PRIMARY);
        }

        char short_title[12];
        strncpy(short_title, win->title, 11);
        short_title[11] = '\0';
        gui_gfx_draw_string(tab_x + 8, ty + 12, short_title,
                            win->minimized ? GUI_THEME_TEXT_DIM
                                           : (win->active ? GUI_THEME_TEXT_MAIN
                                                          : GUI_THEME_TEXT_MUTED));
        tab_x += 106;
    }

    // System tray: CPU trace, memory badge, clock
    for (int i = 0; i < 47; i++) {
        int x0 = tray_x + i;
        int y0 = ty + 26 - (g_cpu_history[i] * 18) / 100;
        int y1 = ty + 26 - (g_cpu_history[i + 1] * 18) / 100;
        gui_gfx_draw_line(x0, y0, x0 + 1, y1, GUI_THEME_SUCCESS);
    }

    uint64_t total = pmm_get_total_pages();
    uint64_t used  = pmm_get_used_pages();
    char buf[24];
    snprintf(buf, sizeof(buf), "%llu MB",
             (unsigned long long)((used * 4096) / (1024 * 1024)));
    gui_gfx_fill_rect(sw - 145, ty + 6, 52, 20, GUI_THEME_SUCCESS);
    gui_gfx_draw_rect(sw - 145, ty + 6, 52, 20, 0xFF059669);
    gui_gfx_draw_string(sw - 140, ty + 12, buf, GUI_COLOR_BLACK);
    (void)total;

    rtc_time_t now;
    rtc_get_time(&now);
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", now.hour, now.minute, now.second);
    gui_gfx_draw_string_16_shadow(sw - 84, ty + 8, buf, GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
}

// ===========================================================================
// Start menu
// ===========================================================================

static int start_menu_height(void) {
    // Header + one row per app + separator + power row.
    return 32 + APP_COUNT * START_ITEM_H + 10 + START_ITEM_H + 8;
}

static void start_menu_origin(int* mx, int* my) {
    *mx = 6;
    *my = gui_desktop_workarea_height() - start_menu_height() - 6;
    if (*my < 4) *my = 4;
}

void gui_desktop_render_start_menu(void) {
    if (!g_start_menu_open) return;

    int mx, my;
    start_menu_origin(&mx, &my);
    int mh = start_menu_height();

    gui_gfx_draw_shadow(mx, my, START_MENU_W, mh, 10);
    gui_gfx_fill_rect(mx, my, START_MENU_W, mh, GUI_THEME_BG_SURFACE);
    gui_gfx_draw_rect(mx, my, START_MENU_W, mh, GUI_THEME_PRIMARY);

    gui_gfx_draw_gradient_v(mx + 1, my + 1, START_MENU_W - 2, 30,
                            GUI_THEME_PRIMARY, GUI_THEME_PRIMARY_DARK);
    gui_gfx_draw_string_16_shadow(mx + 12, my + 7, "Applications",
                                  GUI_COLOR_WHITE, GUI_COLOR_BLACK);

    for (int i = 0; i < APP_COUNT; i++) {
        int iy = my + 32 + i * START_ITEM_H;
        bool hot = (g_hover_menu_item == i);

        if (hot) {
            gui_gfx_fill_rect(mx + 2, iy, START_MENU_W - 4, START_ITEM_H,
                              GUI_THEME_BG_ELEVATED);
            gui_gfx_fill_rect(mx + 2, iy, 3, START_ITEM_H, GUI_THEME_PRIMARY);
        }

        gui_icon_draw(g_apps[i].icon, mx + 12, iy + (START_ITEM_H - 16) / 2);
        gui_gfx_draw_string(mx + 36, iy + (START_ITEM_H - 8) / 2, g_apps[i].label,
                            hot ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED);
    }

    int sep_y = my + 32 + APP_COUNT * START_ITEM_H + 4;
    gui_gfx_draw_line(mx + 8, sep_y, mx + START_MENU_W - 9, sep_y, GUI_THEME_BORDER);

    int power_y = sep_y + 6;
    bool power_hot = (g_hover_menu_item == APP_COUNT);
    if (power_hot) {
        gui_gfx_fill_rect(mx + 2, power_y, START_MENU_W - 4, START_ITEM_H, 0xFF3B1418);
        gui_gfx_fill_rect(mx + 2, power_y, 3, START_ITEM_H, GUI_THEME_DANGER);
    }
    gui_icon_draw(GUI_ICON_POWER, mx + 12, power_y + (START_ITEM_H - 16) / 2);
    gui_gfx_draw_string(mx + 36, power_y + (START_ITEM_H - 8) / 2,
                        "Exit to Kernel TTY", GUI_THEME_DANGER);
}

// ===========================================================================
// Desktop context menu
// ===========================================================================

static const char* const g_context_items[] = {
    "Tile Windows", "Cascade Windows", "Toggle Grid", "New Terminal", "About SUB-OS"
};
#define CONTEXT_ITEM_COUNT 5

void gui_desktop_open_context_menu(int mx, int my) {
    g_context_menu_open = true;
    g_start_menu_open   = false;

    int menu_h = CONTEXT_ITEM_COUNT * CONTEXT_ITEM_H + 8;
    g_context_x = mx;
    g_context_y = my;

    // Flip the menu back on screen when opened near an edge.
    if (g_context_x + CONTEXT_MENU_W > gui_gfx_get_width()) {
        g_context_x = gui_gfx_get_width() - CONTEXT_MENU_W - 2;
    }
    if (g_context_y + menu_h > gui_desktop_workarea_height()) {
        g_context_y = gui_desktop_workarea_height() - menu_h - 2;
    }
}

void gui_desktop_render_context_menu(void) {
    if (!g_context_menu_open) return;

    int mh = CONTEXT_ITEM_COUNT * CONTEXT_ITEM_H + 8;
    gui_gfx_draw_shadow(g_context_x, g_context_y, CONTEXT_MENU_W, mh, 6);
    gui_gfx_fill_rect(g_context_x, g_context_y, CONTEXT_MENU_W, mh, GUI_THEME_BG_SURFACE);
    gui_gfx_draw_rect(g_context_x, g_context_y, CONTEXT_MENU_W, mh, GUI_THEME_ACCENT);

    for (int i = 0; i < CONTEXT_ITEM_COUNT; i++) {
        int iy = g_context_y + 4 + i * CONTEXT_ITEM_H;
        bool hot = (g_hover_menu_item == 100 + i);

        if (hot) {
            gui_gfx_fill_rect(g_context_x + 2, iy, CONTEXT_MENU_W - 4,
                              CONTEXT_ITEM_H, GUI_THEME_BG_ELEVATED);
        }
        gui_gfx_draw_string(g_context_x + 12, iy + (CONTEXT_ITEM_H - 8) / 2,
                            g_context_items[i],
                            hot ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED);
    }
}

void gui_desktop_toggle_start_menu(void) {
    g_start_menu_open = !g_start_menu_open;
    if (g_start_menu_open) g_context_menu_open = false;
}

bool gui_desktop_is_start_menu_open(void) { return g_start_menu_open; }

void gui_desktop_close_menus(void) {
    g_start_menu_open = false;
    g_context_menu_open = false;
}

void gui_desktop_request_exit(void) { g_desktop_running = false; }

static void exit_confirm_cb(gui_dialog_result_t result, void* ctx) {
    (void)ctx;
    if (result == GUI_DIALOG_RESULT_OK) g_desktop_running = false;
}

// ===========================================================================
// Input routing
// ===========================================================================

// Track which menu row the pointer is over so rendering can highlight it.
static void update_menu_hover(int mx, int my) {
    g_hover_menu_item = -1;

    if (g_start_menu_open) {
        int smx, smy;
        start_menu_origin(&smx, &smy);

        if (mx >= smx && mx < smx + START_MENU_W) {
            int rel = my - (smy + 32);
            if (rel >= 0) {
                int idx = rel / START_ITEM_H;
                if (idx >= 0 && idx < APP_COUNT) {
                    g_hover_menu_item = idx;
                } else {
                    int power_y = smy + 32 + APP_COUNT * START_ITEM_H + 10;
                    if (my >= power_y && my < power_y + START_ITEM_H) {
                        g_hover_menu_item = APP_COUNT;
                    }
                }
            }
        }
        return;
    }

    if (g_context_menu_open) {
        if (mx >= g_context_x && mx < g_context_x + CONTEXT_MENU_W) {
            int rel = my - (g_context_y + 4);
            int idx = rel / CONTEXT_ITEM_H;
            if (rel >= 0 && idx < CONTEXT_ITEM_COUNT) g_hover_menu_item = 100 + idx;
        }
    }
}

static bool handle_start_menu_click(int mx, int my) {
    int smx, smy;
    start_menu_origin(&smx, &smy);
    int mh = start_menu_height();

    if (mx < smx || mx >= smx + START_MENU_W || my < smy || my >= smy + mh) {
        g_start_menu_open = false;
        return true;   // The dismissing click is consumed, not passed through.
    }

    int rel = my - (smy + 32);
    int idx = rel / START_ITEM_H;

    if (rel >= 0 && idx >= 0 && idx < APP_COUNT) {
        g_start_menu_open = false;
        launch_app(idx);
        return true;
    }

    int power_y = smy + 32 + APP_COUNT * START_ITEM_H + 10;
    if (my >= power_y && my < power_y + START_ITEM_H) {
        g_start_menu_open = false;
        gui_dialog_confirm("Exit Desktop",
                           "Leave the graphical session and return to the kernel TTY?",
                           exit_confirm_cb, NULL);
        return true;
    }

    return true;
}

static bool handle_context_menu_click(int mx, int my) {
    int mh = CONTEXT_ITEM_COUNT * CONTEXT_ITEM_H + 8;

    if (mx < g_context_x || mx >= g_context_x + CONTEXT_MENU_W ||
        my < g_context_y || my >= g_context_y + mh) {
        g_context_menu_open = false;
        return true;
    }

    int rel = my - (g_context_y + 4);
    g_context_menu_open = false;
    if (rel < 0) return true;

    int idx = rel / CONTEXT_ITEM_H;

    switch (idx) {
        case 0: gui_wm_tile_windows(); break;
        case 1: gui_wm_cascade_windows(); break;
        case 2: g_show_grid = !g_show_grid; break;
        case 3: launch_app(0); break;
        case 4: launch_app(APP_COUNT - 1); break;
        default: break;
    }
    return true;
}

static bool handle_taskbar_click(int mx, int my) {
    int ty = gui_desktop_workarea_height();

    if (mx >= 6 && mx < 98 && my >= ty + 4 && my < ty + 28) {
        gui_desktop_toggle_start_menu();
        return true;
    }

    int tab_x = 106;
    int tray_x = gui_gfx_get_width() - 200;
    for (int i = 0; i < gui_wm_get_window_count() && tab_x < tray_x - 106; i++) {
        gui_window_t* win = gui_wm_get_window_at_index(i);
        if (!win || !win->visible) continue;

        if (mx >= tab_x && mx < tab_x + 100 && my >= ty + 4 && my < ty + 28) {
            // Clicking the focused window's button minimizes it; otherwise
            // the window is restored and raised.
            if (win->active && !win->minimized) {
                gui_wm_minimize_window(win->id);
            } else {
                gui_wm_restore_window(win->id);
                gui_wm_focus_window(win->id);
            }
            return true;
        }
        tab_x += 106;
    }

    return true; // Bare taskbar clicks are absorbed
}

bool gui_desktop_handle_click(int mx, int my) {
    if (g_start_menu_open)   return handle_start_menu_click(mx, my);
    if (g_context_menu_open) return handle_context_menu_click(mx, my);

    if (my >= gui_desktop_workarea_height()) return handle_taskbar_click(mx, my);

    // Icons only claim the click when no window covers that spot.
    for (int i = gui_wm_get_window_count() - 1; i >= 0; i--) {
        gui_window_t* win = gui_wm_get_window_at_index(i);
        if (!win || !win->visible || win->minimized) continue;
        if (mx >= win->x && mx < win->x + win->width &&
            my >= win->y && my < win->y + win->height) {
            return false;
        }
    }

    int icon = icon_hit_test(mx, my);
    if (icon >= 0) {
        if (g_selected_icon == icon) {
            // Second click on an already-selected icon launches it.
            launch_app(g_desktop_icons[icon]);
            g_selected_icon = -1;
        } else {
            g_selected_icon = icon;
        }
        return true;
    }

    g_selected_icon = -1;
    return false;
}

// ===========================================================================
// Frame loop
// ===========================================================================

static void sample_cpu_history(void) {
    // Shift in a fresh sample derived from the current P-state.
    for (int i = 0; i < 47; i++) g_cpu_history[i] = g_cpu_history[i + 1];

    cpufreq_stats_t cf = cpufreq_get_stats();
    uint32_t span = (cf.max_khz > cf.min_khz) ? (cf.max_khz - cf.min_khz) : 1;
    uint32_t rel  = (cf.current_khz > cf.min_khz) ? (cf.current_khz - cf.min_khz) : 0;
    uint32_t pct  = (rel * 100) / span;
    if (pct > 100) pct = 100;

    g_cpu_history[47] = (uint8_t)pct;
}

int gui_desktop_run(void) {
    fbcon_enable(false);
    gui_desktop_init();

    // Open a default session so the desktop is not empty on first boot.
    launch_app(2);   // System Monitor
    launch_app(0);   // Terminal

    // Discard anything the console left in the keyboard queue; a stale key
    // arriving on frame 0 would otherwise act as a desktop shortcut.
    for (int drained = 0; drained < 16 && keyboard_has_key(); drained++) {
        (void)keyboard_get_key();
    }

    g_desktop_running = true;
    printk(KERN_INFO "GUI: Desktop session started (ESC returns to the kernel TTY)\n");

    while (g_desktop_running) {
        // --- 1. Sample input -----------------------------------------------
        const mouse_state_t* ms = mouse_get_state();
        int  mx    = ms ? ms->x : gui_gfx_get_width() / 2;
        int  my    = ms ? ms->y : gui_gfx_get_height() / 2;
        bool left  = ms ? ms->left_btn : false;
        bool right = ms ? ms->right_btn : false;

        gui_cursor_set_pos(mx, my);
        bool click_edge       = left && !g_prev_left;
        bool right_click_edge = right && !g_prev_right;

        // --- 2. Route the pointer, modal layer first ------------------------
        if (gui_dialog_is_open()) {
            gui_dialog_handle_mouse(mx, my, click_edge);
            gui_wm_sync_button_state(left);
        } else {
            update_menu_hover(mx, my);

            if (right_click_edge && my < gui_desktop_workarea_height()) {
                gui_desktop_open_context_menu(mx, my);
                gui_wm_sync_button_state(left);
            } else if (click_edge) {
                if (gui_desktop_handle_click(mx, my)) {
                    gui_wm_sync_button_state(left);
                } else {
                    gui_wm_handle_mouse(mx, my, left, right);
                }
            } else {
                gui_wm_handle_mouse(mx, my, left, right);
            }
        }

        g_prev_left  = left;
        g_prev_right = right;

        // --- 3. Route the keyboard ------------------------------------------
        // keyboard_get_key() blocks until a key arrives, which would freeze
        // the compositor, so only call it once the driver reports one ready.
        uint16_t key = keyboard_has_key() ? keyboard_get_key() : 0;
        if (key != 0) {
            if (gui_dialog_is_open()) {
                gui_dialog_handle_key(key);
            } else if ((char)(key & 0xFF) == 27) {
                if (g_start_menu_open || g_context_menu_open) {
                    gui_desktop_close_menus();
                } else {
                    gui_dialog_confirm("Exit Desktop",
                                       "Leave the graphical session and return to "
                                       "the kernel TTY?", exit_confirm_cb, NULL);
                }
            } else {
                gui_wm_handle_key(key);
            }
        }

        // --- 4. Composite the scene, back to front --------------------------
        if ((g_frames & 0x1F) == 0) sample_cpu_history();

        gui_desktop_render_background();
        gui_desktop_render_icons();
        gui_wm_render();
        gui_desktop_render_taskbar();
        gui_desktop_render_start_menu();
        gui_desktop_render_context_menu();
        gui_dialog_render();
        gui_cursor_draw();

        gui_gfx_present();
        g_frames++;
    }

    // Hand the screen to the framebuffer console. Without this the shell writes
    // into the VGA text buffer while the adapter is still scanning out the
    // framebuffer, so the TTY is visible only on the serial line -- which is
    // the host terminal, not the emulator window.
    fbcon_enable(true);

    printk(KERN_INFO "GUI: Desktop session ended after %llu frames, TTY restored\n",
           (unsigned long long)g_frames);
    return 0;
}
