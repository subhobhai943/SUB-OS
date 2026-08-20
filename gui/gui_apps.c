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
static uint8_t g_cpu_history[60] = {
    12, 15, 18, 22, 30, 25, 20, 35, 45, 50, 42, 38, 55, 60, 48, 52, 35, 30, 25, 20,
    28, 32, 45, 60, 55, 40, 30, 25, 20, 18, 22, 25, 15, 18, 22, 30, 35, 40, 38, 42,
    30, 25, 20, 18, 15, 22, 28, 35, 40, 48, 52, 45, 38, 30, 25, 20, 18, 15, 20, 24
};

static void sysmon_paint(gui_window_t* win) {
    int cx = win->x + 12;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 10;

    gui_gfx_draw_string_16_shadow(cx, cy, "CPU Load History (Real-Time)", GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
    
    // CPU Graph Box
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

    // Plot Waveform
    for (int i = 0; i < 59 && (i * 5 + 5) < graph_w; i++) {
        int y1 = graph_y + graph_h - 3 - (g_cpu_history[i] * (graph_h - 6)) / 100;
        int y2 = graph_y + graph_h - 3 - (g_cpu_history[i + 1] * (graph_h - 6)) / 100;
        gui_gfx_draw_line(graph_x + 4 + i * 5, y1, graph_x + 4 + (i + 1) * 5, y2, GUI_THEME_SUCCESS);
    }

    // Memory Usage Bar
    int mem_y = graph_y + graph_h + 12;
    gui_gfx_draw_string_16(cx, mem_y, "RAM Usage: 24 MB / 128 MB (18% Heap)", GUI_THEME_TEXT_MAIN);
    
    int bar_y = mem_y + 20;
    int bar_w = win->width - 24;
    gui_gfx_fill_rect(cx, bar_y, bar_w, 12, GUI_THEME_BG_DARK);
    gui_gfx_fill_rect(cx, bar_y, (bar_w * 24) / 128, 12, GUI_THEME_ACCENT);
    gui_gfx_draw_rect(cx, bar_y, bar_w, 12, GUI_THEME_BORDER);

    // System Telemetry Metrics
    char buf[64];
    cpufreq_stats_t stats = cpufreq_get_stats();
    snprintf(buf, sizeof(buf), "Clock: %u.%03u GHz | Governor: ondemand",
             stats.current_khz / 1000000, (stats.current_khz % 1000000) / 1000);
    gui_gfx_draw_string(cx, bar_y + 20, buf, GUI_THEME_TEXT_MUTED);

    snprintf(buf, sizeof(buf), "Core Tasks: 6 Active | VFS Cache: 98.4%% Hit", 0);
    gui_gfx_draw_string(cx, bar_y + 32, buf, GUI_THEME_TEXT_DIM);
}

void gui_app_sysmon_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("System Monitor", x, y, (w > 0) ? w : 330, (h > 0) ? h : 230);
    if (win) {
        win->paint = sysmon_paint;
    }
}

// =================================================================
// 2. GUI Terminal Emulator App (High-Res 440x280)
// =================================================================
typedef struct {
    char lines[12][50];
    int line_count;
    char input[48];
    int input_len;
} term_data_t;

static void terminal_paint(gui_window_t* win) {
    term_data_t* td = (term_data_t*)win->user_data;
    if (!td) return;

    int cx = win->x + 10;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 8;

    gui_gfx_fill_rect(win->x + 1, win->y + GUI_TITLEBAR_HEIGHT + 1, win->width - 2, win->height - GUI_TITLEBAR_HEIGHT - 2, 0xFF0B0F19);

    // Render lines with sharp 8x16 font
    for (int i = 0; i < td->line_count && i < 11; i++) {
        uint32_t fg = (td->lines[i][0] == '$') ? GUI_THEME_SUCCESS : GUI_THEME_TEXT_MAIN;
        gui_gfx_draw_string_16(cx, cy + i * 18, td->lines[i], fg);
    }

    // Render active prompt line
    int prompt_y = cy + td->line_count * 18;
    if (prompt_y < win->y + win->height - 20) {
        gui_gfx_draw_string_16(cx, prompt_y, "sub-os:/> ", GUI_THEME_PRIMARY);
        gui_gfx_draw_string_16(cx + 80, prompt_y, td->input, GUI_THEME_TEXT_MAIN);
        // Blinking Cursor block
        gui_gfx_fill_rect(cx + 80 + td->input_len * 8, prompt_y + 2, 8, 14, GUI_THEME_SUCCESS);
    }
}

static void terminal_event(gui_window_t* win, const gui_event_t* ev) {
    term_data_t* td = (term_data_t*)win->user_data;
    if (!td) return;

    if (ev->type == GUI_EVENT_KEY_DOWN) {
        char c = (char)(ev->key & 0xFF);
        if (c == '\n' || c == '\r') {
            if (td->line_count < 10) {
                snprintf(td->lines[td->line_count], 50, "$ %s", td->input);
                td->line_count++;
                if (strcmp(td->input, "clear") == 0) {
                    td->line_count = 0;
                } else if (strcmp(td->input, "uname") == 0) {
                    snprintf(td->lines[td->line_count++], 50, "SUB-OS 0.2.0-lts (x86_64)");
                } else if (strcmp(td->input, "help") == 0) {
                    snprintf(td->lines[td->line_count++], 50, "Commands: uname, clear, date, tree, help");
                } else if (strcmp(td->input, "tree") == 0) {
                    snprintf(td->lines[td->line_count++], 50, "/ [bin etc dev proc sys home mnt]");
                } else if (td->input_len > 0) {
                    snprintf(td->lines[td->line_count++], 50, "Executed: '%s'", td->input);
                }
            } else {
                for (int i = 0; i < 9; i++) strcpy(td->lines[i], td->lines[i + 1]);
                snprintf(td->lines[9], 50, "$ %s", td->input);
            }
            td->input[0] = '\0';
            td->input_len = 0;
        } else if (c == '\b' || (uint8_t)c == 0x7F) {
            if (td->input_len > 0) {
                td->input_len--;
                td->input[td->input_len] = '\0';
            }
        } else if (c >= 32 && c <= 126 && td->input_len < 36) {
            td->input[td->input_len++] = c;
            td->input[td->input_len] = '\0';
        }
    } else if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) {
            kfree(win->user_data);
            win->user_data = NULL;
        }
    }
}

void gui_app_terminal_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Terminal Console", x, y, (w > 0) ? w : 440, (h > 0) ? h : 270);
    if (win) {
        term_data_t* td = (term_data_t*)kzalloc(sizeof(term_data_t));
        strcpy(td->lines[0], "SUB-OS High-Resolution Terminal Emulator");
        strcpy(td->lines[1], "Type 'uname', 'tree', or 'help'");
        td->line_count = 2;
        win->user_data = td;
        win->paint = terminal_paint;
        win->handle_event = terminal_event;
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
