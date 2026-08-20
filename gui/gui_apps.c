// Standard GUI Application Suite for SUB-OS Desktop
#include <gui/gui_apps.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <drivers/rtc.h>
#include <drivers/cpufreq.h>
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
// 1. System Performance Monitor App
// =================================================================
static uint8_t g_cpu_history[32] = {15, 20, 18, 30, 25, 40, 35, 28, 45, 50, 42, 38, 55, 60, 48, 52, 35, 30, 25, 20, 28, 32, 45, 60, 55, 40, 30, 25, 20, 18, 22, 25};

static void sysmon_paint(gui_window_t* win) {
    int cx = win->x + 6;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 6;

    gui_gfx_draw_string(cx, cy, "CPU Load History:", GUI_THEME_PRIMARY);
    
    // CPU Graph Box
    int graph_x = cx;
    int graph_y = cy + 12;
    int graph_w = win->width - 12;
    int graph_h = 32;
    gui_gfx_fill_rect(graph_x, graph_y, graph_w, graph_h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(graph_x, graph_y, graph_w, graph_h, GUI_THEME_BORDER);

    // Draw Grid Lines
    for (int gy = graph_y + 8; gy < graph_y + graph_h; gy += 8) {
        gui_gfx_draw_line(graph_x + 1, gy, graph_x + graph_w - 2, gy, 0xFF1E293B);
    }

    // Plot CPU Graph
    for (int i = 0; i < 31 && (i * 4 + 4) < graph_w; i++) {
        int y1 = graph_y + graph_h - 2 - (g_cpu_history[i] * (graph_h - 4)) / 100;
        int y2 = graph_y + graph_h - 2 - (g_cpu_history[i + 1] * (graph_h - 4)) / 100;
        gui_gfx_draw_line(graph_x + 2 + i * 4, y1, graph_x + 2 + (i + 1) * 4, y2, GUI_THEME_SUCCESS);
    }

    // Memory Usage Bar
    int mem_y = graph_y + graph_h + 8;
    gui_gfx_draw_string(cx, mem_y, "RAM Heap: 24 MB / 128 MB", GUI_THEME_TEXT_MAIN);
    
    int bar_y = mem_y + 12;
    int bar_w = win->width - 12;
    gui_gfx_fill_rect(cx, bar_y, bar_w, 8, GUI_THEME_BG_DARK);
    gui_gfx_fill_rect(cx, bar_y, (bar_w * 24) / 128, 8, GUI_THEME_ACCENT);
    gui_gfx_draw_rect(cx, bar_y, bar_w, 8, GUI_THEME_BORDER);

    // CPU Frequency & Architecture Specs
    char spec_buf[48];
    cpufreq_stats_t stats = cpufreq_get_stats();
    snprintf(spec_buf, sizeof(spec_buf), "Freq: %u.%03u GHz (%s)",
             stats.current_khz / 1000000, (stats.current_khz % 1000000) / 1000,
             "ondemand");
    gui_gfx_draw_string(cx, bar_y + 12, spec_buf, GUI_THEME_TEXT_MUTED);
}

void gui_app_sysmon_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("System Monitor", x, y, (w > 0) ? w : 150, (h > 0) ? h : 115);
    if (win) {
        win->paint = sysmon_paint;
    }
}

// =================================================================
// 2. GUI Terminal Emulator App
// =================================================================
typedef struct {
    char lines[6][32];
    int line_count;
    char input[32];
    int input_len;
} term_data_t;

static void terminal_paint(gui_window_t* win) {
    term_data_t* td = (term_data_t*)win->user_data;
    if (!td) return;

    int cx = win->x + 4;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 4;

    gui_gfx_fill_rect(win->x + 1, win->y + GUI_TITLEBAR_HEIGHT + 1, win->width - 2, win->height - GUI_TITLEBAR_HEIGHT - 2, 0xFF050811);

    // Render historical lines
    for (int i = 0; i < td->line_count && i < 6; i++) {
        gui_gfx_draw_string(cx, cy + i * 10, td->lines[i], GUI_THEME_TEXT_MAIN);
    }

    // Render prompt and active input
    int prompt_y = cy + td->line_count * 10;
    if (prompt_y < win->y + win->height - 12) {
        gui_gfx_draw_string(cx, prompt_y, "$ ", GUI_THEME_SUCCESS);
        gui_gfx_draw_string(cx + 16, prompt_y, td->input, GUI_THEME_TEXT_MAIN);
        // Cursor
        gui_gfx_fill_rect(cx + 16 + td->input_len * 8, prompt_y + 1, 6, 8, GUI_THEME_PRIMARY);
    }
}

static void terminal_event(gui_window_t* win, const gui_event_t* ev) {
    term_data_t* td = (term_data_t*)win->user_data;
    if (!td) return;

    if (ev->type == GUI_EVENT_KEY_DOWN) {
        char c = (char)(ev->key & 0xFF);
        if (c == '\n' || c == '\r') {
            if (td->line_count < 5) {
                snprintf(td->lines[td->line_count], 32, "$ %s", td->input);
                td->line_count++;
                if (strcmp(td->input, "clear") == 0) {
                    td->line_count = 0;
                } else if (strcmp(td->input, "uname") == 0) {
                    snprintf(td->lines[td->line_count++], 32, "SUB-OS 0.2.0-lts x86_64");
                } else if (strcmp(td->input, "help") == 0) {
                    snprintf(td->lines[td->line_count++], 32, "cmds: uname, clear, help");
                } else if (td->input_len > 0) {
                    snprintf(td->lines[td->line_count++], 32, "cmd: ok");
                }
            } else {
                for (int i = 0; i < 4; i++) strcpy(td->lines[i], td->lines[i + 1]);
                snprintf(td->lines[4], 32, "$ %s", td->input);
            }
            td->input[0] = '\0';
            td->input_len = 0;
        } else if (c == '\b') {
            if (td->input_len > 0) {
                td->input_len--;
                td->input[td->input_len] = '\0';
            }
        } else if (c >= 32 && c <= 126 && td->input_len < 20) {
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
    gui_window_t* win = gui_wm_create_window("Terminal", x, y, (w > 0) ? w : 160, (h > 0) ? h : 100);
    if (win) {
        term_data_t* td = (term_data_t*)kzalloc(sizeof(term_data_t));
        strcpy(td->lines[0], "SUB-OS Graphical TTY");
        strcpy(td->lines[1], "Type 'help' or 'uname'");
        td->line_count = 2;
        win->user_data = td;
        win->paint = terminal_paint;
        win->handle_event = terminal_event;
    }
}

// =================================================================
// 3. File Manager Explorer App
// =================================================================
static void fileman_paint(gui_window_t* win) {
    int cx = win->x + 6;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 6;

    gui_gfx_draw_string(cx, cy, "Location: /", GUI_THEME_PRIMARY);
    gui_gfx_draw_line(win->x + 2, cy + 10, win->x + win->width - 3, cy + 10, GUI_THEME_BORDER);

    const char* items[] = {
        "[D] bin/",
        "[D] etc/",
        "[D] dev/",
        "[D] proc/",
        "[D] sys/",
        "[D] mnt/",
        "[F] readme.txt",
        "[F] kernel.elf"
    };

    for (int i = 0; i < 8; i++) {
        int ix = cx + (i % 2) * 65;
        int iy = cy + 14 + (i / 2) * 12;
        if (iy + 10 > win->y + win->height) break;
        uint32_t col = (items[i][1] == 'D') ? GUI_THEME_ACCENT : GUI_THEME_TEXT_MAIN;
        gui_gfx_draw_string(ix, iy, items[i], col);
    }
}

void gui_app_fileman_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("File Explorer", x, y, (w > 0) ? w : 140, (h > 0) ? h : 95);
    if (win) {
        win->paint = fileman_paint;
    }
}

// =================================================================
// 4. GUI Calculator App
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

    int cx = win->x + 4;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 4;

    // Display Box
    gui_gfx_fill_rect(cx, cy, win->width - 8, 14, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(cx, cy, win->width - 8, 14, GUI_THEME_BORDER);
    gui_gfx_draw_string(cx + 4, cy + 3, cd->display, GUI_THEME_SUCCESS);

    // Button Grid (4x4)
    const char* btns[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };

    int btn_w = (win->width - 16) / 4;
    int btn_h = 10;
    int start_y = cy + 18;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = cx + c * (btn_w + 2);
            int by = start_y + r * (btn_h + 2);
            uint32_t bg = (c == 3 || (r == 3 && c == 2)) ? GUI_THEME_PRIMARY_DARK : GUI_THEME_BG_ELEVATED;
            gui_gfx_fill_rect(bx, by, btn_w, btn_h, bg);
            gui_gfx_draw_rect(bx, by, btn_w, btn_h, GUI_THEME_BORDER);
            gui_gfx_draw_string(bx + (btn_w - 8) / 2, by + 1, btns[r][c], GUI_THEME_TEXT_MAIN);
        }
    }
}

static void calc_event(gui_window_t* win, const gui_event_t* ev) {
    calc_data_t* cd = (calc_data_t*)win->user_data;
    if (!cd) return;

    if (ev->type == GUI_EVENT_MOUSE_DOWN && ev->btn_left) {
        int btn_w = (win->width - 16) / 4;
        int btn_h = 10;
        int start_y = 22; // Relative to client area
        int rel_y = ev->rel_y - start_y;
        int rel_x = ev->rel_x - 4;

        if (rel_y >= 0 && rel_x >= 0) {
            int row = rel_y / (btn_h + 2);
            int col = rel_x / (btn_w + 2);

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
                    } else if (strlen(cd->display) < 8) {
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
    gui_window_t* win = gui_wm_create_window("Calculator", x, y, (w > 0) ? w : 110, (h > 0) ? h : 85);
    if (win) {
        calc_data_t* cd = (calc_data_t*)kzalloc(sizeof(calc_data_t));
        strcpy(cd->display, "0");
        win->user_data = cd;
        win->paint = calc_paint;
        win->handle_event = calc_event;
    }
}

// =================================================================
// 5. Paint / Canvas Draw App
// =================================================================
typedef struct {
    uint32_t selected_color;
} paint_data_t;

static void paint_app_paint(gui_window_t* win) {
    paint_data_t* pd = (paint_data_t*)win->user_data;
    if (!pd) return;

    int cx = win->x + 2;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 2;

    // Palette Bar
    uint32_t colors[] = {GUI_THEME_PRIMARY, GUI_THEME_SUCCESS, GUI_THEME_WARNING, GUI_THEME_DANGER, GUI_COLOR_WHITE, GUI_COLOR_BLACK};
    for (int i = 0; i < 6; i++) {
        gui_gfx_fill_rect(cx + i * 14, cy, 12, 8, colors[i]);
        gui_gfx_draw_rect(cx + i * 14, cy, 12, 8, (colors[i] == pd->selected_color) ? GUI_THEME_PRIMARY : GUI_THEME_BORDER);
    }
}

static void paint_app_event(gui_window_t* win, const gui_event_t* ev) {
    paint_data_t* pd = (paint_data_t*)win->user_data;
    if (!pd) return;

    if (ev->btn_left) {
        if (ev->rel_y <= 10) {
            uint32_t colors[] = {GUI_THEME_PRIMARY, GUI_THEME_SUCCESS, GUI_THEME_WARNING, GUI_THEME_DANGER, GUI_COLOR_WHITE, GUI_COLOR_BLACK};
            int idx = (ev->rel_x - 2) / 14;
            if (idx >= 0 && idx < 6) {
                pd->selected_color = colors[idx];
            }
        } else {
            // Draw on canvas
            gui_gfx_fill_rect(win->x + ev->rel_x - 1, win->y + GUI_TITLEBAR_HEIGHT + ev->rel_y - 1, 3, 3, pd->selected_color);
        }
    } else if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) {
            kfree(win->user_data);
            win->user_data = NULL;
        }
    }
}

void gui_app_paint_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Paint Studio", x, y, (w > 0) ? w : 130, (h > 0) ? h : 90);
    if (win) {
        paint_data_t* pd = (paint_data_t*)kzalloc(sizeof(paint_data_t));
        pd->selected_color = GUI_THEME_PRIMARY;
        win->user_data = pd;
        win->paint = paint_app_paint;
        win->handle_event = paint_app_event;
    }
}

// =================================================================
// 6. About SUB-OS Dialog App
// =================================================================
static void about_paint(gui_window_t* win) {
    int cx = win->x + 8;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 8;

    gui_gfx_draw_string(cx, cy, "SUB-OS Desktop v0.2.0", GUI_THEME_PRIMARY);
    gui_gfx_draw_string(cx, cy + 12, "Modular Monolithic Kernel", GUI_THEME_TEXT_MAIN);
    gui_gfx_draw_string(cx, cy + 24, "Author: subhobhai943", GUI_THEME_TEXT_MUTED);
    gui_gfx_draw_string(cx, cy + 36, "C++17 / Rust / SUB Engine", GUI_THEME_SUCCESS);
}

void gui_app_about_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("About SUB-OS", x, y, (w > 0) ? w : 150, (h > 0) ? h : 75);
    if (win) {
        win->paint = about_paint;
    }
}
