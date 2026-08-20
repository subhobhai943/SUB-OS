// High-Resolution Modern Desktop Environment & Compositor for SUB-OS (800x600)
#include <gui/gui_desktop.h>
#include <gui/gui_gfx.h>
#include <gui/gui_cursor.h>
#include <gui/gui_wm.h>
#include <gui/gui_apps.h>
#include <gui/gui_theme.h>
#include <drivers/fb.h>
#include <drivers/mouse.h>
#include <drivers/keyboard.h>
#include <drivers/rtc.h>
#include <drivers/cpufreq.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

static bool g_start_menu_open = false;
static bool g_desktop_running = false;
static bool g_prev_left = false;

void gui_desktop_init(void) {
    const fb_info_t* fb = fb_get_info();
    uint32_t* fb_addr = fb ? fb->address : NULL;
    int w = (fb && fb->width > 0) ? fb->width : 800;
    int h = (fb && fb->height > 0) ? fb->height : 600;

    gui_gfx_init(fb_addr, w, h);
    gui_cursor_init();
    gui_wm_init();
    mouse_set_bounds(w, h);
    g_start_menu_open = false;
}

void gui_desktop_render_background(void) {
    int sw = gui_gfx_get_width();
    int sh = gui_gfx_get_height();

    // Dark Cyberpunk / Nord Deep Gradient Wallpaper
    gui_gfx_draw_gradient_v(0, 0, sw, sh - GUI_TASKBAR_HEIGHT, 0xFF0B132B, 0xFF1C2541);

    // Subtle Architectural Grid Overlay
    for (int y = 0; y < sh - GUI_TASKBAR_HEIGHT; y += 40) {
        gui_gfx_draw_line(0, y, sw - 1, y, 0xFF141F36);
    }
    for (int x = 0; x < sw; x += 40) {
        gui_gfx_draw_line(x, 0, x, sh - GUI_TASKBAR_HEIGHT - 1, 0xFF141F36);
    }

    // Centered Elegant Desktop Branding
    gui_gfx_draw_string_16_shadow(sw / 2 - 80, sh / 2 - 40, "SUB-OS DESKTOP", 0xFF38BDF8, 0xFF0284C7);
    gui_gfx_draw_string_16(sw / 2 - 95, sh / 2 - 18, "Modular Monolithic Kernel", 0xFF94A3B8);
    gui_gfx_draw_string(sw / 2 - 60, sh / 2 + 6, "v0.2.0-LTS (x86_64)", 0xFF64748B);
}

void gui_desktop_render_taskbar(void) {
    int sw = gui_gfx_get_width();
    int sh = gui_gfx_get_height();
    int ty = sh - GUI_TASKBAR_HEIGHT;

    // Taskbar Surface
    gui_gfx_fill_rect(0, ty, sw, GUI_TASKBAR_HEIGHT, GUI_THEME_TASKBAR_BG);
    gui_gfx_draw_line(0, ty, sw - 1, ty, GUI_THEME_BORDER);

    // Start Button
    uint32_t start_bg = g_start_menu_open ? GUI_THEME_PRIMARY_DARK : GUI_THEME_PRIMARY;
    gui_gfx_fill_rect(6, ty + 4, 86, 24, start_bg);
    gui_gfx_draw_rect(6, ty + 4, 86, 24, GUI_THEME_PRIMARY_DARK);
    gui_gfx_draw_string_16_shadow(14, ty + 8, "# SUB-OS", GUI_COLOR_WHITE, GUI_COLOR_BLACK);

    // Window Tabs in Taskbar
    int tab_x = 100;
    int win_count = gui_wm_get_window_count();
    for (int i = 0; i < win_count && tab_x < sw - 160; i++) {
        gui_window_t* win = gui_wm_get_window(i + 1);
        if (!win || !win->visible) continue;

        uint32_t tab_bg = win->active ? GUI_THEME_BG_ELEVATED : GUI_THEME_BG_SURFACE;
        gui_gfx_fill_rect(tab_x, ty + 4, 100, 24, tab_bg);
        gui_gfx_draw_rect(tab_x, ty + 4, 100, 24, win->active ? GUI_THEME_PRIMARY : GUI_THEME_BORDER);

        char short_title[12];
        strncpy(short_title, win->title, 11);
        short_title[11] = '\0';
        gui_gfx_draw_string_16(tab_x + 8, ty + 8, short_title, win->active ? GUI_THEME_TEXT_MAIN : GUI_THEME_TEXT_MUTED);

        tab_x += 106;
    }

    // System Tray (Clock & Telemetry)
    rtc_time_t time;
    rtc_get_time(&time);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", time.hour, time.minute, time.second);
    gui_gfx_draw_string_16_shadow(sw - 85, ty + 8, time_str, GUI_THEME_PRIMARY, GUI_COLOR_BLACK);

    // RAM Badge
    gui_gfx_fill_rect(sw - 140, ty + 6, 45, 20, GUI_THEME_SUCCESS);
    gui_gfx_draw_rect(sw - 140, ty + 6, 45, 20, 0xFF059669);
    gui_gfx_draw_string_16(sw - 134, ty + 8, "24 MB", GUI_COLOR_WHITE);
}

void gui_desktop_render_start_menu(void) {
    if (!g_start_menu_open) return;

    int sh = gui_gfx_get_height();
    int menu_w = 170;
    int menu_h = 200;
    int mx = 6;
    int my = sh - GUI_TASKBAR_HEIGHT - menu_h - 6;

    // Drop Shadow & Body
    gui_gfx_draw_shadow(mx, my, menu_w, menu_h, 8);
    gui_gfx_fill_rect(mx, my, menu_w, menu_h, GUI_THEME_BG_SURFACE);
    gui_gfx_draw_rect(mx, my, menu_w, menu_h, GUI_THEME_PRIMARY);

    // Menu Header
    gui_gfx_fill_rect(mx + 1, my + 1, menu_w - 2, 26, GUI_THEME_PRIMARY_DARK);
    gui_gfx_draw_string_16_shadow(mx + 10, my + 6, "SUB-OS Applications", GUI_COLOR_WHITE, GUI_COLOR_BLACK);

    const char* items[] = {
        ">_ Terminal Console",
        "[#] System Monitor",
        "[F] File Explorer",
        "[=] Calculator",
        "[*] Paint Studio",
        "[i] About SUB-OS",
        "[X] Exit to TTY"
    };

    for (int i = 0; i < 7; i++) {
        int iy = my + 34 + i * 23;
        uint32_t fg = (i == 6) ? GUI_THEME_DANGER : GUI_THEME_TEXT_MAIN;
        gui_gfx_draw_string_16(mx + 10, iy, items[i], fg);
    }
}

void gui_desktop_toggle_start_menu(void) {
    g_start_menu_open = !g_start_menu_open;
}

bool gui_desktop_is_start_menu_open(void) {
    return g_start_menu_open;
}

void gui_desktop_handle_taskbar_click(int mx, int my) {
    int sw = gui_gfx_get_width();
    int sh = gui_gfx_get_height();
    int ty = sh - GUI_TASKBAR_HEIGHT;

    // Check Start Button
    if (mx >= 6 && mx <= 92 && my >= ty + 4 && my <= ty + 28) {
        gui_desktop_toggle_start_menu();
        return;
    }

    // Check Start Menu Click if open
    if (g_start_menu_open) {
        int menu_w = 170;
        int menu_h = 200;
        int smx = 6;
        int smy = sh - GUI_TASKBAR_HEIGHT - menu_h - 6;

        if (mx >= smx && mx <= smx + menu_w && my >= smy && my <= smy + menu_h) {
            int item_idx = (my - (smy + 34)) / 23;
            g_start_menu_open = false;
            switch (item_idx) {
                case 0: gui_app_terminal_launch(30, 30, 440, 270); break;
                case 1: gui_app_sysmon_launch(420, 30, 330, 230); break;
                case 2: gui_app_fileman_launch(60, 60, 360, 220); break;
                case 3: gui_app_calc_launch(120, 80, 220, 220); break;
                case 4: gui_app_paint_launch(160, 50, 340, 240); break;
                case 5: gui_app_about_launch(200, 100, 320, 160); break;
                case 6: g_desktop_running = false; break; // Exit GUI
                default: break;
            }
            return;
        } else {
            g_start_menu_open = false;
        }
    }

    // Check Window Tabs
    int tab_x = 100;
    int win_count = gui_wm_get_window_count();
    for (int i = 0; i < win_count && tab_x < sw - 160; i++) {
        if (mx >= tab_x && mx <= tab_x + 100 && my >= ty + 4 && my <= ty + 28) {
            gui_wm_focus_window(i + 1);
            return;
        }
        tab_x += 106;
    }
}

int gui_desktop_run(void) {
    gui_desktop_init();

    // Launch default initial windows in spacious 800x600 layout
    gui_app_sysmon_launch(430, 20, 340, 240);
    gui_app_terminal_launch(20, 30, 390, 260);

    g_desktop_running = true;
    printk(KERN_INFO "GUI: Desktop Environment session started (Press ESC to return to TTY)\n");

    while (g_desktop_running) {
        // 1. Process Mouse Input
        const mouse_state_t* ms = mouse_get_state();
        int mx = ms ? ms->x : 400;
        int my = ms ? ms->y : 300;
        bool left = ms ? ms->left_btn : false;
        bool right = ms ? ms->right_btn : false;

        gui_cursor_set_pos(mx, my);

        if (left && !g_prev_left) {
            int sh = gui_gfx_get_height();
            if (my >= sh - GUI_TASKBAR_HEIGHT || g_start_menu_open) {
                gui_desktop_handle_taskbar_click(mx, my);
            } else {
                gui_wm_handle_mouse(mx, my, left, right);
            }
        } else {
            gui_wm_handle_mouse(mx, my, left, right);
        }
        g_prev_left = left;

        // 2. Process Keyboard Input
        uint16_t key = keyboard_get_key();
        if (key != 0) {
            char c = (char)(key & 0xFF);
            if (c == 27) { // Escape key
                g_desktop_running = false;
                break;
            }
            gui_wm_handle_key(key);
        }

        // 3. Render Complete Desktop Scene
        gui_desktop_render_background();
        gui_wm_render();
        gui_desktop_render_taskbar();
        gui_desktop_render_start_menu();
        gui_cursor_draw();

        // 4. Present Framebuffer to Display
        gui_gfx_present();
    }

    printk(KERN_INFO "GUI: Desktop session ended, restored TTY console\n");
    return 0;
}
