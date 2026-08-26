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
#include <kernel/rust.h>
#include <kernel/nt/ob.h>
#include <kernel/nt/reg.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/cpp_analytics.h>
#include <kernel/ktime.h>

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

// =================================================================
// 7. Conway's Game of Life (interactive cellular automaton)
// =================================================================
#define LIFE_COLS 56
#define LIFE_ROWS 34
#define LIFE_TOOLBAR_H 24
#define LIFE_STATUS_H  22

typedef struct {
    uint8_t cur[LIFE_ROWS][LIFE_COLS];
    uint8_t nxt[LIFE_ROWS][LIFE_COLS];
    bool     running;
    uint32_t generation;
    uint32_t frame;
} life_data_t;

// Seed the grid from the Rust ChaCha20 CSPRNG (~28% of cells alive).
static void life_randomize(life_data_t* ld) {
    // Static (not on the stack): the desktop is single-threaded, and this keeps
    // ~1.9 KB off the kernel stack that the GUI thread runs on.
    static uint8_t rnd[LIFE_ROWS * LIFE_COLS];
    if (rust_csprng_get_random(rnd, sizeof(rnd)) != 0) {
        // Deterministic fallback if the CSPRNG is unavailable.
        for (size_t i = 0; i < sizeof(rnd); i++) {
            rnd[i] = (uint8_t)((i * 2654435761u) >> 24);
        }
    }
    for (int r = 0; r < LIFE_ROWS; r++) {
        for (int c = 0; c < LIFE_COLS; c++) {
            ld->cur[r][c] = ((rnd[r * LIFE_COLS + c] & 7) < 2) ? 1 : 0;
        }
    }
    ld->generation = 0;
}

// One tick of the B3/S23 rule with dead borders.
static void life_step(life_data_t* ld) {
    for (int r = 0; r < LIFE_ROWS; r++) {
        for (int c = 0; c < LIFE_COLS; c++) {
            int n = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int rr = r + dr, cc = c + dc;
                    if (rr >= 0 && rr < LIFE_ROWS && cc >= 0 && cc < LIFE_COLS && ld->cur[rr][cc]) {
                        n++;
                    }
                }
            }
            uint8_t alive = ld->cur[r][c];
            ld->nxt[r][c] = ((alive && (n == 2 || n == 3)) || (!alive && n == 3)) ? 1 : 0;
        }
    }
    memcpy(ld->cur, ld->nxt, sizeof(ld->cur));
    ld->generation++;
}

// Grid cell size and top-left, derived from the current window geometry so the
// board scales when the window is resized. Kept identical in paint and event.
static void life_geometry(gui_window_t* win, int* cell, int* gw, int* gh, int* gx_rel, int* grid_top_rel) {
    int avail_w = win->width - 16;
    int avail_h = win->height - GUI_TITLEBAR_HEIGHT - LIFE_TOOLBAR_H - LIFE_STATUS_H;
    int cw = avail_w / LIFE_COLS;
    int ch = avail_h / LIFE_ROWS;
    int sz = (cw < ch) ? cw : ch;
    if (sz < 2) sz = 2;
    *cell = sz;
    *gw = sz * LIFE_COLS;
    *gh = sz * LIFE_ROWS;
    *gx_rel = (win->width - *gw) / 2;
    *grid_top_rel = LIFE_TOOLBAR_H;
}

static const char* const LIFE_BTNS[4] = { "Play", "Step", "Random", "Clear" };
#define LIFE_BTN_W 66
#define LIFE_BTN_GAP 72

static void life_paint(gui_window_t* win) {
    life_data_t* ld = (life_data_t*)win->user_data;
    if (!ld) return;

    int cx  = win->x + 8;
    int top = win->y + GUI_TITLEBAR_HEIGHT;

    // Toolbar
    for (int i = 0; i < 4; i++) {
        int bx = cx + i * LIFE_BTN_GAP;
        int by = top + 3;
        gui_gfx_fill_rect(bx, by, LIFE_BTN_W, 18, GUI_THEME_BG_DARK);
        gui_gfx_draw_rect(bx, by, LIFE_BTN_W, 18, GUI_THEME_BORDER);
        const char* label = (i == 0 && ld->running) ? "Pause" : LIFE_BTNS[i];
        int tw = (int)strlen(label) * 8;
        gui_gfx_draw_string(bx + (LIFE_BTN_W - tw) / 2, by + 5, label, GUI_THEME_TEXT_MAIN);
    }

    // Board
    int cell, gw, gh, gx_rel, grid_top_rel;
    life_geometry(win, &cell, &gw, &gh, &gx_rel, &grid_top_rel);
    int gx = win->x + gx_rel;
    int gy = top + grid_top_rel;

    gui_gfx_fill_rect(gx, gy, gw, gh, 0xFF0B1220);
    int pop = 0;
    for (int r = 0; r < LIFE_ROWS; r++) {
        for (int c = 0; c < LIFE_COLS; c++) {
            if (ld->cur[r][c]) {
                pop++;
                gui_gfx_fill_rect(gx + c * cell, gy + r * cell,
                                  (cell > 2) ? cell - 1 : cell,
                                  (cell > 2) ? cell - 1 : cell, GUI_THEME_SUCCESS);
            }
        }
    }
    gui_gfx_draw_rect(gx, gy, gw, gh, GUI_THEME_BORDER);

    // Status line
    char buf[96];
    snprintf(buf, sizeof(buf), "Gen %u   Pop %d   %s   -- click cells to toggle",
             ld->generation, pop, ld->running ? "RUNNING" : "paused");
    gui_gfx_draw_string(cx, gy + gh + 5, buf, GUI_THEME_TEXT_MUTED);

    // Advance the automaton while running, throttled so it is watchable.
    if (ld->running) {
        ld->frame++;
        if ((ld->frame & 3) == 0) {
            life_step(ld);
        }
    }
}

static void life_event(gui_window_t* win, const gui_event_t* ev) {
    life_data_t* ld = (life_data_t*)win->user_data;
    if (!ld) return;

    if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) {
            kfree(win->user_data);
            win->user_data = NULL;
        }
        return;
    }

    // Act only on the press edge so buttons and cells do not re-fire while held.
    if (ev->type != GUI_EVENT_MOUSE_DOWN) return;

    int rel_x = ev->rel_x;
    int rel_y = ev->rel_y;

    // Toolbar row
    if (rel_y >= 3 && rel_y < LIFE_TOOLBAR_H) {
        int i = (rel_x - 8) / LIFE_BTN_GAP;
        int within = (rel_x - 8) - i * LIFE_BTN_GAP;
        if (i >= 0 && i < 4 && within >= 0 && within < LIFE_BTN_W) {
            switch (i) {
                case 0: ld->running = !ld->running; break;
                case 1: life_step(ld); break;
                case 2: life_randomize(ld); break;
                case 3: memset(ld->cur, 0, sizeof(ld->cur)); ld->generation = 0; break;
            }
        }
        return;
    }

    // Board: toggle the clicked cell
    int cell, gw, gh, gx_rel, grid_top_rel;
    life_geometry(win, &cell, &gw, &gh, &gx_rel, &grid_top_rel);
    int lx = rel_x - gx_rel;
    int ly = rel_y - grid_top_rel;
    if (lx >= 0 && ly >= 0 && lx < gw && ly < gh) {
        int c = lx / cell;
        int r = ly / cell;
        if (r >= 0 && r < LIFE_ROWS && c >= 0 && c < LIFE_COLS) {
            ld->cur[r][c] ^= 1;
        }
    }
}

void gui_app_life_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Game of Life", x, y, (w > 0) ? w : 480, (h > 0) ? h : 380);
    if (win) {
        life_data_t* ld = (life_data_t*)kzalloc(sizeof(life_data_t));
        if (ld) {
            life_randomize(ld);
            ld->running = true;
        }
        win->user_data = ld;
        win->paint = life_paint;
        win->handle_event = life_event;
    }
}

// =================================================================
// 8. Rust Kernel Lab -- showcases the memory-safe Rust subsystem
// =================================================================
typedef struct {
    rust_crypto_bench_result_t bench; // measured once at launch
    bool     have_bench;
    uint32_t frame;
    uint8_t  entropy[8];              // live CSPRNG sample
} rustlab_data_t;

static void rustlab_bar(int x, int y, int w, const char* label, uint32_t value,
                        uint32_t max_value, uint32_t color) {
    char buf[48];
    gui_gfx_draw_string(x, y, label, GUI_THEME_TEXT_MAIN);
    int bx = x + 96;
    int bw = w - 96 - 64;
    if (bw < 20) bw = 20;
    gui_gfx_fill_rect(bx, y, bw, 10, GUI_THEME_BG_DARK);
    uint32_t denom = (max_value > 0) ? max_value : 1;
    int fill = (int)(((uint64_t)value * (uint64_t)bw) / denom);
    if (fill > bw) fill = bw;
    gui_gfx_fill_rect(bx, y, fill, 10, color);
    gui_gfx_draw_rect(bx, y, bw, 10, GUI_THEME_BORDER);
    snprintf(buf, sizeof(buf), "%u MB/s", value);
    gui_gfx_draw_string(bx + bw + 6, y, buf, GUI_THEME_TEXT_MUTED);
}

static void rustlab_paint(gui_window_t* win) {
    rustlab_data_t* rd = (rustlab_data_t*)win->user_data;
    if (!rd) return;

    int cx = win->x + 14;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 12;
    int w  = win->width - 28;

    gui_gfx_draw_string_16_shadow(cx, cy, "Rust Kernel Subsystem (no_std)", GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
    cy += 26;

    // Checksum engine (recomputed live -- it is cheap)
    static const char sample[] = "SUB-OS";
    uint32_t crc = rust_crc32c((const uint8_t*)sample, 6);
    uint32_t adl = rust_adler32((const uint8_t*)sample, 6);
    char buf[80];
    gui_gfx_draw_string(cx, cy, "Checksums of \"SUB-OS\":", GUI_THEME_SUCCESS);
    cy += 14;
    snprintf(buf, sizeof(buf), "CRC-32C 0x%08X   Adler-32 0x%08X", crc, adl);
    gui_gfx_draw_string(cx + 8, cy, buf, GUI_THEME_TEXT_MUTED);
    cy += 22;

    // Crypto benchmark bars (measured once at launch)
    gui_gfx_draw_string(cx, cy, "Crypto throughput (measured):", GUI_THEME_SUCCESS);
    cy += 16;
    if (rd->have_bench) {
        uint32_t mx = rd->bench.chacha20_mbs;
        if (rd->bench.sha3_256_mbs > mx) mx = rd->bench.sha3_256_mbs;
        if (rd->bench.aes128_mbs > mx)   mx = rd->bench.aes128_mbs;
        rustlab_bar(cx + 4, cy, w, "ChaCha20", rd->bench.chacha20_mbs, mx, GUI_THEME_ACCENT);
        cy += 16;
        rustlab_bar(cx + 4, cy, w, "SHA3-256", rd->bench.sha3_256_mbs, mx, GUI_THEME_SUCCESS);
        cy += 16;
        rustlab_bar(cx + 4, cy, w, "AES-128", rd->bench.aes128_mbs, mx, GUI_THEME_WARNING);
        cy += 16;
        snprintf(buf, sizeof(buf), "Composite score: %u", rd->bench.score);
        gui_gfx_draw_string(cx + 4, cy, buf, GUI_THEME_TEXT_DIM);
        cy += 22;
    } else {
        gui_gfx_draw_string(cx + 4, cy, "benchmark unavailable", GUI_THEME_TEXT_DIM);
        cy += 22;
    }

    // Live CSPRNG entropy, refreshed periodically
    if ((rd->frame++ % 20) == 0) {
        rust_csprng_get_random(rd->entropy, sizeof(rd->entropy));
    }
    gui_gfx_draw_string(cx, cy, "ChaCha20 CSPRNG stream:", GUI_THEME_SUCCESS);
    cy += 14;
    snprintf(buf, sizeof(buf), "%02X %02X %02X %02X %02X %02X %02X %02X",
             rd->entropy[0], rd->entropy[1], rd->entropy[2], rd->entropy[3],
             rd->entropy[4], rd->entropy[5], rd->entropy[6], rd->entropy[7]);
    gui_gfx_draw_string(cx + 8, cy, buf, GUI_THEME_PRIMARY);
}

static void rustlab_event(gui_window_t* win, const gui_event_t* ev) {
    if (ev->type == GUI_EVENT_CLOSE && win->user_data) {
        kfree(win->user_data);
        win->user_data = NULL;
    }
}

void gui_app_rustlab_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Rust Lab", x, y, (w > 0) ? w : 420, (h > 0) ? h : 300);
    if (win) {
        rustlab_data_t* rd = (rustlab_data_t*)kzalloc(sizeof(rustlab_data_t));
        if (rd) {
            rd->have_bench = (rust_crypto_run_benchmark(&rd->bench) == 0);
            rust_csprng_get_random(rd->entropy, sizeof(rd->entropy));
        }
        win->user_data = rd;
        win->paint = rustlab_paint;
        win->handle_event = rustlab_event;
    }
}

// =================================================================
// 9. NT Object Browser -- a WinObj-style view of the object namespace
// =================================================================
typedef struct { ob_object_t* obj; int depth; } objrow_t;
#define OBJBROWSE_MAX_ROWS 96

typedef struct {
    int scroll;
} objbrowse_data_t;

static int objbrowse_flatten(ob_object_t* obj, int depth, objrow_t* rows, int max, int count) {
    if (!obj || count >= max) return count;
    rows[count].obj = obj;
    rows[count].depth = depth;
    count++;
    if (obj->type == OB_TYPE_DIRECTORY) {
        int c = ob_dir_child_count(obj);
        for (int i = 0; i < c && count < max; i++) {
            count = objbrowse_flatten(ob_dir_child(obj, i), depth + 1, rows, max, count);
        }
    }
    return count;
}

static uint32_t objbrowse_type_color(ob_type_t t) {
    switch (t) {
        case OB_TYPE_DIRECTORY: return GUI_THEME_PRIMARY;
        case OB_TYPE_EVENT:     return GUI_THEME_SUCCESS;
        case OB_TYPE_SEMAPHORE: return GUI_THEME_WARNING;
        case OB_TYPE_MUTANT:    return GUI_THEME_ACCENT;
        default:                return GUI_THEME_TEXT_MUTED;
    }
}

static void objbrowse_paint(gui_window_t* win) {
    objbrowse_data_t* bd = (objbrowse_data_t*)win->user_data;
    if (!bd) return;

    int cx  = win->x + 12;
    int top = win->y + GUI_TITLEBAR_HEIGHT;

    // Header + live stats
    uint32_t objs = 0, handles = 0, per[OB_TYPE_MAX];
    ob_get_stats(&objs, &handles, per);
    gui_gfx_draw_string_16_shadow(cx, top + 8, "NT Object Namespace  \\", GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
    char hdr[80];
    snprintf(hdr, sizeof(hdr), "%u objects   %u handles open   (scroll: click top/bottom)", objs, handles);
    gui_gfx_draw_string(cx, top + 28, hdr, GUI_THEME_TEXT_MUTED);

    // Flatten the live namespace
    objrow_t rows[OBJBROWSE_MAX_ROWS];
    int total = objbrowse_flatten(ob_root_directory(), 0, rows, OBJBROWSE_MAX_ROWS, 0);

    int list_top = top + 44;
    int row_h = 13;
    int visible = (win->height - (list_top - win->y) - 10) / row_h;
    if (visible < 1) visible = 1;

    if (bd->scroll > total - visible) bd->scroll = total - visible;
    if (bd->scroll < 0) bd->scroll = 0;

    for (int i = 0; i < visible && (bd->scroll + i) < total; i++) {
        objrow_t* r = &rows[bd->scroll + i];
        ob_object_t* o = r->obj;
        int y = list_top + i * row_h;
        int x = cx + r->depth * 14;

        char line[96];
        if (o->type == OB_TYPE_DIRECTORY) {
            snprintf(line, sizeof(line), "%s\\", o->name);
        } else {
            snprintf(line, sizeof(line), "%s", o->name);
        }
        gui_gfx_draw_string(x, y, line, objbrowse_type_color(o->type));

        char meta[48];
        snprintf(meta, sizeof(meta), "%-9s r=%d h=%d", ob_type_name(o->type), o->ref_count, o->handle_count);
        gui_gfx_draw_string(win->x + win->width - 180, y, meta, GUI_THEME_TEXT_DIM);
    }

    // Scroll indicator
    if (total > visible) {
        char sc[32];
        snprintf(sc, sizeof(sc), "%d-%d / %d", bd->scroll + 1,
                 (bd->scroll + visible < total) ? bd->scroll + visible : total, total);
        gui_gfx_draw_string(win->x + win->width - 90, top + 28, sc, GUI_THEME_TEXT_DIM);
    }
}

static void objbrowse_event(gui_window_t* win, const gui_event_t* ev) {
    objbrowse_data_t* bd = (objbrowse_data_t*)win->user_data;
    if (!bd) return;

    if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) { kfree(win->user_data); win->user_data = NULL; }
        return;
    }
    if (ev->type != GUI_EVENT_MOUSE_DOWN) return;

    // Top third scrolls up, bottom third scrolls down.
    if (ev->rel_y < win->height / 3) {
        bd->scroll -= 3;
    } else if (ev->rel_y > (win->height * 2) / 3) {
        bd->scroll += 3;
    }
    if (bd->scroll < 0) bd->scroll = 0;
}

void gui_app_objbrowse_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Object Browser", x, y, (w > 0) ? w : 460, (h > 0) ? h : 340);
    if (win) {
        objbrowse_data_t* bd = (objbrowse_data_t*)kzalloc(sizeof(objbrowse_data_t));
        win->user_data = bd;
        win->paint = objbrowse_paint;
        win->handle_event = objbrowse_event;
    }
}

// =================================================================
// 10. Registry Editor -- navigate the NT \Registry hive tree
// =================================================================
typedef struct {
    reg_key_t* current;
    int        row_h;
    int        list_top_rel;   // list start, relative to window content top
    int        sub_count;      // subkeys shown this frame (for click mapping)
    bool       has_up;         // a ".." row is present
} regedit_data_t;

static void regedit_build_path(reg_key_t* key, char* out, size_t len) {
    // Walk parents into a reversed segment stack, then join with backslashes.
    const char* segs[16];
    int n = 0;
    reg_key_t* k = key;
    while (k && n < 16) { segs[n++] = k->name; k = k->parent; }
    size_t pos = 0;
    out[0] = '\0';
    for (int i = n - 1; i >= 0; i--) {
        int wrote = snprintf(out + pos, (pos < len) ? len - pos : 0, "\\%s", segs[i]);
        if (wrote > 0) pos += (size_t)wrote;
        if (pos >= len) break;
    }
}

static void regedit_paint(gui_window_t* win) {
    regedit_data_t* rd = (regedit_data_t*)win->user_data;
    if (!rd) return;
    if (!rd->current) rd->current = reg_root();

    int cx  = win->x + 12;
    int top = win->y + GUI_TITLEBAR_HEIGHT;

    char path[192];
    regedit_build_path(rd->current, path, sizeof(path));
    gui_gfx_draw_string_16_shadow(cx, top + 8, "Registry Editor", GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
    gui_gfx_draw_string(cx, top + 28, path, GUI_THEME_TEXT_MUTED);

    rd->row_h = 13;
    rd->list_top_rel = GUI_TITLEBAR_HEIGHT + 46;
    int y = top + 46;

    rd->has_up = (rd->current->parent != NULL);
    if (rd->has_up) {
        gui_gfx_draw_string(cx, y, "..", GUI_THEME_ACCENT);
        y += rd->row_h;
    }

    // Subkeys (navigable)
    int kc = reg_enum_key_count(rd->current);
    rd->sub_count = kc;
    for (int i = 0; i < kc; i++) {
        reg_key_t* sub = reg_enum_key(rd->current, i);
        char line[64];
        snprintf(line, sizeof(line), "[%s]", sub->name);
        gui_gfx_draw_string(cx, y, line, GUI_THEME_SUCCESS);
        y += rd->row_h;
    }

    // Values pane
    y += 4;
    gui_gfx_draw_line(cx, y, win->x + win->width - 12, y, GUI_THEME_BORDER);
    y += 6;
    int vc = reg_enum_value_count(rd->current);
    if (vc == 0) {
        gui_gfx_draw_string(cx, y, "(no values)", GUI_THEME_TEXT_DIM);
    }
    for (int i = 0; i < vc; i++) {
        reg_value_t* v = reg_enum_value(rd->current, i);
        char rendered[REG_STR_MAX + 8];
        reg_value_to_string(v, rendered, sizeof(rendered));
        char line[160];
        snprintf(line, sizeof(line), "%-18s %-10s %s", v->name, reg_type_name(v->type), rendered);
        gui_gfx_draw_string(cx, y, line, GUI_THEME_TEXT_MAIN);
        y += rd->row_h;
        if (y > win->y + win->height - 12) break;
    }
}

static void regedit_event(gui_window_t* win, const gui_event_t* ev) {
    regedit_data_t* rd = (regedit_data_t*)win->user_data;
    if (!rd) return;

    if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) { kfree(win->user_data); win->user_data = NULL; }
        return;
    }
    if (ev->type != GUI_EVENT_MOUSE_DOWN) return;
    if (!rd->current) return;

    int rel = ev->rel_y - rd->list_top_rel;
    if (rel < 0) return;
    int row = rel / (rd->row_h > 0 ? rd->row_h : 13);

    int idx = 0;
    if (rd->has_up) {
        if (row == 0) { rd->current = rd->current->parent; return; }
        idx = row - 1;
    } else {
        idx = row;
    }
    if (idx >= 0 && idx < rd->sub_count) {
        reg_key_t* sub = reg_enum_key(rd->current, idx);
        if (sub) rd->current = sub;
    }
}

void gui_app_regedit_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Registry Editor", x, y, (w > 0) ? w : 480, (h > 0) ? h : 360);
    if (win) {
        regedit_data_t* rd = (regedit_data_t*)kzalloc(sizeof(regedit_data_t));
        if (rd) rd->current = reg_root();
        win->user_data = rd;
        win->paint = regedit_paint;
        win->handle_event = regedit_event;
    }
}

// =================================================================
// 12. Analytics -- live charts backed by the C++ Analytics Engine
// =================================================================
typedef struct {
    uint64_t last_sample_ns;   // throttle: pull a new sample ~2 Hz
    int      selected;         // -1 = grid overview, else single-channel focus
} analytics_data_t;

#define ANALYTICS_SAMPLE_INTERVAL_NS 500000000ULL // 0.5s

static uint32_t analytics_channel_color(int ch) {
    switch (ch) {
        case 0: return GUI_THEME_PRIMARY;  // CPU
        case 1: return GUI_THEME_ACCENT;   // Memory
        case 2: return GUI_THEME_WARNING;  // Heap
        case 3: return GUI_THEME_SUCCESS;  // Network
        default: return GUI_THEME_TEXT_MUTED;
    }
}

// Draw one channel's line graph inside the rectangle (gx,gy,gw,gh).
static void analytics_draw_graph(int ch, int gx, int gy, int gw, int gh, bool detailed) {
    uint32_t color = analytics_channel_color(ch);

    // Panel background + border
    gui_gfx_fill_rect(gx, gy, gw, gh, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(gx, gy, gw, gh, GUI_THEME_BORDER);

    // Header: name + last value
    uint32_t mn, mx, avg, last, sd;
    cpp_analytics_get_stats(ch, &mn, &mx, &avg, &last, &sd);
    uint32_t scale = cpp_analytics_channel_scale(ch);
    if (scale == 0) scale = 1;

    char hdr[48];
    snprintf(hdr, sizeof(hdr), "%s", cpp_analytics_channel_name(ch));
    gui_gfx_draw_string(gx + 6, gy + 5, hdr, color);
    char val[24];
    snprintf(val, sizeof(val), "%u%s", last, cpp_analytics_channel_unit(ch));
    gui_gfx_draw_string(gx + gw - 8 * (int)strlen(val) - 6, gy + 5, val, GUI_THEME_TEXT_MAIN);

    // Plot area
    int px = gx + 6;
    int py = gy + 20;
    int pw = gw - 12;
    int ph = gh - (detailed ? 40 : 34);
    if (ph < 8) ph = 8;

    // Horizontal gridlines (25/50/75%)
    for (int g = 1; g < 4; g++) {
        int gyl = py + (ph * g) / 4;
        gui_gfx_draw_line(px, gyl, px + pw, gyl, GUI_THEME_BG_ELEVATED);
    }

    // Fetch the series and draw it as a connected line with a soft fill.
    uint32_t series[CPP_ANALYTICS_HISTORY];
    int n = cpp_analytics_get_series(ch, series, CPP_ANALYTICS_HISTORY);
    if (n >= 1) {
        int prev_x = 0, prev_y = 0;
        for (int i = 0; i < n; i++) {
            int x = px + (n > 1 ? (pw * i) / (n - 1) : pw);
            uint32_t v = series[i];
            if (v > scale) v = scale;
            int y = py + ph - (int)(((uint64_t)v * ph) / scale);
            // vertical fill to baseline
            gui_gfx_draw_line(x, y, x, py + ph, (color & 0x00FFFFFF) | 0x30000000);
            if (i > 0) gui_gfx_draw_line(prev_x, prev_y, x, y, color);
            prev_x = x; prev_y = y;
        }
    }

    // Footer stats
    char foot[64];
    if (detailed) {
        snprintf(foot, sizeof(foot), "min %u  avg %u  max %u  sd %u", mn, avg, mx, sd);
    } else {
        snprintf(foot, sizeof(foot), "min %u  avg %u  max %u", mn, avg, mx);
    }
    gui_gfx_draw_string(gx + 6, gy + gh - 12, foot, GUI_THEME_TEXT_DIM);
}

static void analytics_paint(gui_window_t* win) {
    analytics_data_t* ad = (analytics_data_t*)win->user_data;
    if (!ad) return;

    // Throttled live sampling driven by the compositor's repaint cadence.
    uint64_t now = ktime_ns();
    if (now - ad->last_sample_ns >= ANALYTICS_SAMPLE_INTERVAL_NS) {
        cpp_analytics_sample();
        ad->last_sample_ns = now;
    }

    int cx  = win->x + 10;
    int top = win->y + GUI_TITLEBAR_HEIGHT + 4;

    char title[72];
    snprintf(title, sizeof(title), "C++ Analytics Engine   %llu samples",
             (unsigned long long)cpp_analytics_sample_count());
    gui_gfx_draw_string_16_shadow(cx, top, title, GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
    gui_gfx_draw_string(cx, top + 20, "Templated time-series + Observer pattern (kernel/cpp)", GUI_THEME_TEXT_MUTED);

    int grid_top = top + 38;
    int nch = cpp_analytics_channel_count();

    if (ad->selected >= 0 && ad->selected < nch) {
        // Focused single-channel view.
        analytics_draw_graph(ad->selected, cx, grid_top,
                             win->width - 20, win->height - (grid_top - win->y) - 10, true);
        gui_gfx_draw_string(cx + 6, win->y + win->height - 12,
                            "(click to return to overview)", GUI_THEME_TEXT_DIM);
    } else {
        // 2x2 overview grid.
        int gw = (win->width - 20 - 8) / 2;
        int gh = (win->height - (grid_top - win->y) - 10 - 8) / 2;
        if (gw < 60) gw = 60;
        if (gh < 40) gh = 40;
        for (int i = 0; i < nch && i < 4; i++) {
            int col = i % 2, row = i / 2;
            analytics_draw_graph(i, cx + col * (gw + 8), grid_top + row * (gh + 8), gw, gh, false);
        }
    }
}

static void analytics_event(gui_window_t* win, const gui_event_t* ev) {
    analytics_data_t* ad = (analytics_data_t*)win->user_data;
    if (!ad) return;

    if (ev->type == GUI_EVENT_CLOSE) {
        if (win->user_data) { kfree(win->user_data); win->user_data = NULL; }
        return;
    }
    if (ev->type != GUI_EVENT_MOUSE_DOWN) return;

    if (ad->selected >= 0) {
        ad->selected = -1; // any click leaves focus mode
        return;
    }
    // Overview: figure out which quadrant was clicked and focus it.
    int grid_top_rel = GUI_TITLEBAR_HEIGHT + 4 + 38;
    int gw = (win->width - 20 - 8) / 2;
    int gh = (win->height - grid_top_rel - 10 - 8) / 2;
    if (gw < 1 || gh < 1) return;
    int rx = ev->rel_x - 10;
    int ry = ev->rel_y - grid_top_rel;
    if (rx < 0 || ry < 0) return;
    int col = rx / (gw + 8);
    int row = ry / (gh + 8);
    if (col > 1) col = 1;
    if (row > 1) row = 1;
    int idx = row * 2 + col;
    if (idx >= 0 && idx < cpp_analytics_channel_count()) ad->selected = idx;
}

void gui_app_analytics_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Analytics", x, y, (w > 0) ? w : 560, (h > 0) ? h : 400);
    if (win) {
        analytics_data_t* ad = (analytics_data_t*)kzalloc(sizeof(analytics_data_t));
        if (ad) { ad->selected = -1; ad->last_sample_ns = 0; }
        win->user_data = ad;
        win->paint = analytics_paint;
        win->handle_event = analytics_event;
    }
}
