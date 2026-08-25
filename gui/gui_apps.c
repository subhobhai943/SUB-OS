// High-Resolution GUI Applications for SUB-OS Desktop (800x600)
#include <gui/gui_apps.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <drivers/rtc.h>
#include <drivers/cpufreq.h>
#include <kernel/tsc.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/printf.h>

static int simple_atoi(const char* s) {
    if (!s) return 0;
    int res = 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

// =================================================================
// 1. System Performance Monitor App (High-Res 320x240)
// =================================================================
/* Rolling trace of live heap utilisation (percent), newest sample on the right.
 * Seeded flat and advanced one sample per repaint from real allocator stats. */
static uint8_t g_heap_history[60];
static bool    g_heap_history_seeded = false;

static void sysmon_sample_heap(uint8_t pct) {
    if (!g_heap_history_seeded) {
        for (int i = 0; i < 60; i++) g_heap_history[i] = pct;
        g_heap_history_seeded = true;
        return;
    }
    for (int i = 0; i < 59; i++) g_heap_history[i] = g_heap_history[i + 1];
    g_heap_history[59] = pct;
}

static void sysmon_paint(gui_window_t* win) {
    int cx = win->x + 12;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 10;

    /* Pull live figures straight from the kernel allocators. */
    uint64_t heap_used  = (uint64_t)heap_get_used_bytes();
    uint64_t heap_total = (uint64_t)heap_get_total_bytes();
    uint64_t heap_free  = (uint64_t)heap_get_free_bytes();
    uint64_t heap_grows = (uint64_t)heap_get_grow_count();
    uint32_t heap_pct   = heap_total ? (uint32_t)((heap_used * 100) / heap_total) : 0;
    if (heap_pct > 100) heap_pct = 100;

    uint64_t ram_free_mb  = (pmm_get_free_pages()  * PMM_PAGE_SIZE) / (1024 * 1024);
    uint64_t ram_total_mb = pmm_get_usable_memory() / (1024 * 1024);

    sysmon_sample_heap((uint8_t)heap_pct);

    gui_gfx_draw_string_16_shadow(cx, cy, "Heap Utilization History (Live)", GUI_THEME_PRIMARY, GUI_COLOR_BLACK);

    // Heap Graph Box
    int graph_x = cx;
    int graph_y = cy + 20;
    int graph_w = win->width - 24;
    int graph_h = 70;
    gui_gfx_fill_rect(graph_x, graph_y, graph_w, graph_h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(graph_x, graph_y, graph_w, graph_h, GUI_THEME_BORDER);

    // Plot Grid Lines
    for (int gy = graph_y + 14; gy < graph_y + graph_h; gy += 14) {
        gui_gfx_draw_line(graph_x + 1, gy, graph_x + graph_w - 2, gy, 0xFF1E293B);
    }
    for (int gx = graph_x + 30; gx < graph_x + graph_w; gx += 30) {
        gui_gfx_draw_line(gx, graph_y + 1, gx, graph_y + graph_h - 2, 0xFF1E293B);
    }

    // Plot Waveform from the live heap trace
    for (int i = 0; i < 59 && (i * 5 + 5) < graph_w; i++) {
        int y1 = graph_y + graph_h - 3 - (g_heap_history[i] * (graph_h - 6)) / 100;
        int y2 = graph_y + graph_h - 3 - (g_heap_history[i + 1] * (graph_h - 6)) / 100;
        gui_gfx_draw_line(graph_x + 4 + i * 5, y1, graph_x + 4 + (i + 1) * 5, y2, GUI_THEME_SUCCESS);
    }

    // Heap Usage Bar (real used / total)
    char buf[80];
    int mem_y = graph_y + graph_h + 12;
    snprintf(buf, sizeof(buf), "Heap: %llu / %llu KB (%u%%)",
             (unsigned long long)(heap_used / 1024),
             (unsigned long long)(heap_total / 1024), heap_pct);
    gui_gfx_draw_string_16(cx, mem_y, buf, GUI_THEME_TEXT_MAIN);

    int bar_y = mem_y + 20;
    int bar_w = win->width - 24;
    gui_gfx_fill_rect(cx, bar_y, bar_w, 12, GUI_THEME_BG_DARK);
    gui_gfx_fill_rect(cx, bar_y, (int)((uint64_t)bar_w * heap_pct / 100), 12,
                      (heap_pct >= 90) ? GUI_THEME_DANGER : GUI_THEME_ACCENT);
    gui_gfx_draw_rect(cx, bar_y, bar_w, 12, GUI_THEME_BORDER);

    // System Telemetry Metrics (live)
    cpufreq_stats_t stats = cpufreq_get_stats();
    snprintf(buf, sizeof(buf), "Clock: %u.%03u GHz | Heap free: %llu KB | Grows: %llu",
             stats.current_khz / 1000000, (stats.current_khz % 1000000) / 1000,
             (unsigned long long)(heap_free / 1024), (unsigned long long)heap_grows);
    gui_gfx_draw_string(cx, bar_y + 20, buf, GUI_THEME_TEXT_MUTED);

    snprintf(buf, sizeof(buf), "System RAM: %llu MB free / %llu MB usable",
             (unsigned long long)ram_free_mb, (unsigned long long)ram_total_mb);
    gui_gfx_draw_string(cx, bar_y + 32, buf, GUI_THEME_TEXT_DIM);
}

void gui_app_sysmon_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("System Monitor", x, y, (w > 0) ? w : 330, (h > 0) ? h : 230);
    if (win) {
        win->paint = sysmon_paint;
    }
}

// =================================================================
// 3. File Manager Explorer App (High-Res 380x240)
// =================================================================
static void fileman_paint(gui_window_t* win) {
    int cx = win->x + 12;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 10;

    // Address Bar
    gui_gfx_fill_rect(cx, cy, win->width - 24, 22, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(cx, cy, win->width - 24, 22, GUI_THEME_BORDER);
    gui_gfx_draw_string_16(cx + 8, cy + 3, "Path: /", GUI_THEME_PRIMARY);

    // Separator line
    int list_y = cy + 30;
    gui_gfx_draw_line(win->x + 2, list_y, win->x + win->width - 3, list_y, GUI_THEME_BORDER);

    const char* items[] = {
        "[DIR] bin",   "[DIR] etc",
        "[DIR] dev",   "[DIR] proc",
        "[DIR] sys",   "[DIR] home",
        "[DIR] mnt",   "[DIR] usr",
        "[FILE] kernel.elf", "[FILE] readme.txt",
        "[FILE] config.h",   "[FILE] banner.art"
    };

    for (int i = 0; i < 12; i++) {
        int ix = cx + (i % 2) * 160;
        int iy = list_y + 10 + (i / 2) * 22;
        if (iy + 18 > win->y + win->height) break;
        bool is_dir = (items[i][1] == 'D');
        uint32_t col = is_dir ? GUI_THEME_ACCENT : GUI_THEME_TEXT_MAIN;
        gui_gfx_draw_string_16(ix, iy, items[i], col);
    }
}

void gui_app_fileman_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("File Explorer", x, y, (w > 0) ? w : 360, (h > 0) ? h : 220);
    if (win) {
        win->paint = fileman_paint;
    }
}

// =================================================================
// 4. GUI Calculator App (High-Res 220x240)
// =================================================================
typedef struct {
    char display[16];
    int op1;
    char op;
    bool clear_on_digit;
} calc_data_t;

static void calc_paint(gui_window_t* win) {
    calc_data_t* cd = (calc_data_t*)win->user_data;
    if (!cd) return;

    int cx = win->x + 12;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 10;

    // Display Screen
    gui_gfx_fill_rect(cx, cy, win->width - 24, 28, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(cx, cy, win->width - 24, 28, GUI_THEME_BORDER);
    gui_gfx_draw_string_16(cx + 8, cy + 6, cd->display, GUI_THEME_SUCCESS);

    // Button Grid (4x4)
    const char* btns[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };

    int btn_w = (win->width - 36) / 4;
    int btn_h = 28;
    int start_y = cy + 38;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = cx + c * (btn_w + 4);
            int by = start_y + r * (btn_h + 4);
            uint32_t bg = (c == 3 || (r == 3 && c == 2)) ? GUI_THEME_PRIMARY_DARK : GUI_THEME_BG_ELEVATED;
            gui_gfx_fill_rect(bx, by, btn_w, btn_h, bg);
            gui_gfx_draw_rect(bx, by, btn_w, btn_h, GUI_THEME_BORDER);
            gui_gfx_draw_string_16(bx + (btn_w - 8) / 2, by + 6, btns[r][c], GUI_THEME_TEXT_MAIN);
        }
    }
}

static void calc_event(gui_window_t* win, const gui_event_t* ev) {
    calc_data_t* cd = (calc_data_t*)win->user_data;
    if (!cd) return;

    if (ev->type == GUI_EVENT_MOUSE_DOWN && ev->btn_left) {
        int btn_w = (win->width - 36) / 4;
        int btn_h = 28;
        int start_y = 48; // Relative to client area
        int rel_y = ev->rel_y - start_y;
        int rel_x = ev->rel_x - 12;

        if (rel_y >= 0 && rel_x >= 0) {
            int row = rel_y / (btn_h + 4);
            int col = rel_x / (btn_w + 4);

            if (row >= 0 && row < 4 && col >= 0 && col < 4) {
                const char* btns[4][4] = {
                    {"7", "8", "9", "/"},
                    {"4", "5", "6", "*"},
                    {"1", "2", "3", "-"},
                    {"C", "0", "=", "+"}
                };
                char val = btns[row][col][0];
                if (val >= '0' && val <= '9') {
                    if (cd->clear_on_digit || strcmp(cd->display, "0") == 0) {
                        cd->display[0] = val;
                        cd->display[1] = '\0';
                        cd->clear_on_digit = false;
                    } else if (strlen(cd->display) < 10) {
                        int len = strlen(cd->display);
                        cd->display[len] = val;
                        cd->display[len + 1] = '\0';
                    }
                } else if (val == 'C') {
                    strcpy(cd->display, "0");
                    cd->op1 = 0; cd->op = 0;
                } else if (val == '=') {
                    int op2 = simple_atoi(cd->display);
                    int res = 0;
                    if (cd->op == '+') res = cd->op1 + op2;
                    else if (cd->op == '-') res = cd->op1 - op2;
                    else if (cd->op == '*') res = cd->op1 * op2;
                    else if (cd->op == '/') res = (op2 != 0) ? (cd->op1 / op2) : 0;
                    snprintf(cd->display, sizeof(cd->display), "%d", res);
                    cd->clear_on_digit = true;
                } else {
                    cd->op1 = simple_atoi(cd->display);
                    cd->op = val;
                    cd->clear_on_digit = true;
                }
            }
        }
    } else if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) {
            kfree(win->user_data);
            win->user_data = NULL;
        }
    }
}

void gui_app_calc_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Calculator", x, y, (w > 0) ? w : 220, (h > 0) ? h : 220);
    if (win) {
        calc_data_t* cd = (calc_data_t*)kzalloc(sizeof(calc_data_t));
        strcpy(cd->display, "0");
        win->user_data = cd;
        win->paint = calc_paint;
        win->handle_event = calc_event;
    }
}

// =================================================================
// 5. Paint Studio App (High-Res 340x240)
// =================================================================
typedef struct {
    uint32_t selected_color;
} paint_data_t;

static void paint_app_paint(gui_window_t* win) {
    paint_data_t* pd = (paint_data_t*)win->user_data;
    if (!pd) return;

    int cx = win->x + 8;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 8;

    // Palette Color Swatches
    uint32_t colors[] = {GUI_THEME_PRIMARY, GUI_THEME_SUCCESS, GUI_THEME_WARNING, GUI_THEME_DANGER, 0xFFEC4899, GUI_COLOR_WHITE, GUI_COLOR_BLACK};
    for (int i = 0; i < 7; i++) {
        gui_gfx_fill_rect(cx + i * 28, cy, 24, 18, colors[i]);
        gui_gfx_draw_rect(cx + i * 28, cy, 24, 18, (colors[i] == pd->selected_color) ? GUI_COLOR_WHITE : GUI_THEME_BORDER);
    }
    gui_gfx_draw_line(win->x + 2, cy + 24, win->x + win->width - 3, cy + 24, GUI_THEME_BORDER);
}

static void paint_app_event(gui_window_t* win, const gui_event_t* ev) {
    paint_data_t* pd = (paint_data_t*)win->user_data;
    if (!pd) return;

    if (ev->btn_left) {
        if (ev->rel_y <= 24) {
            uint32_t colors[] = {GUI_THEME_PRIMARY, GUI_THEME_SUCCESS, GUI_THEME_WARNING, GUI_THEME_DANGER, 0xFFEC4899, GUI_COLOR_WHITE, GUI_COLOR_BLACK};
            int idx = (ev->rel_x - 8) / 28;
            if (idx >= 0 && idx < 7) {
                pd->selected_color = colors[idx];
            }
        } else {
            // Draw on canvas with 4x4 brush
            gui_gfx_fill_rect(win->x + ev->rel_x - 2, win->y + GUI_TITLEBAR_HEIGHT + ev->rel_y - 2, 4, 4, pd->selected_color);
        }
    } else if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) {
            kfree(win->user_data);
            win->user_data = NULL;
        }
    }
}

void gui_app_paint_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Paint Studio", x, y, (w > 0) ? w : 340, (h > 0) ? h : 240);
    if (win) {
        paint_data_t* pd = (paint_data_t*)kzalloc(sizeof(paint_data_t));
        pd->selected_color = GUI_THEME_PRIMARY;
        win->user_data = pd;
        win->paint = paint_app_paint;
        win->handle_event = paint_app_event;
    }
}

// =================================================================
// 6. About SUB-OS Dialog App (High-Res 320x180)
// =================================================================
static void about_paint(gui_window_t* win) {
    int cx = win->x + 16;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 14;

    gui_gfx_draw_string_16_shadow(cx, cy, "SUB-OS Modern Desktop", GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
    gui_gfx_draw_string_16(cx, cy + 22, "Version 0.2.0-LTS (x86_64)", GUI_THEME_TEXT_MAIN);
    gui_gfx_draw_string(cx, cy + 48, "Architecture : Modular Monolithic Kernel", GUI_THEME_TEXT_MUTED);
    gui_gfx_draw_string(cx, cy + 62, "Languages    : Modern C++17, Rust, SUB-Lang", GUI_THEME_SUCCESS);
    gui_gfx_draw_string(cx, cy + 76, "Author       : subhobhai943", GUI_THEME_TEXT_DIM);
    gui_gfx_draw_string(cx, cy + 90, "Display      : 800x600 TrueColor Double-Buffered", GUI_THEME_ACCENT);
}

void gui_app_about_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("About SUB-OS", x, y, (w > 0) ? w : 320, (h > 0) ? h : 160);
    if (win) {
        win->paint = about_paint;
    }
}
