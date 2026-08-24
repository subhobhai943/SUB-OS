// SUB-OS Desktop Applications, second generation
//
// These apps are written entirely against the SUB-WT immediate-mode toolkit:
// each one keeps a small state struct in win->user_data, forwards WM events
// into the toolkit, and rebuilds its interface every frame from that state.

#include <gui/gui_apps_ext.h>
#include <gui/gui_widgets.h>
#include <gui/gui_icons.h>
#include <gui/gui_dialog.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <kernel/ktest.h>
#include <kernel/printk.h>
#include <kernel/task.h>
#include <kernel/rcu.h>
#include <kernel/futex.h>
#include <kernel/tsc.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/page_cache.h>
#include <drivers/rtc.h>
#include <drivers/cpufreq.h>
#include <drivers/mouse.h>
#include <drivers/keyboard.h>
#include <arch/arch.h>
#include <lib/string.h>
#include <lib/printf.h>

// Shared plumbing: every app forwards WM events into the toolkit, and frees
// its state struct when the window closes.
static void app_forward_event(gui_window_t* win, const gui_event_t* ev) {
    gui_widget_feed_event(win, ev);
    if (ev->type == GUI_EVENT_CLOSE && win->user_data) {
        kfree(win->user_data);
        win->user_data = NULL;
    }
}

// ===========================================================================
// 1. Settings
// ===========================================================================
typedef struct {
    int  active_tab;
    bool show_grid;
    bool window_shadows;
    bool sound_enabled;
    bool show_clock_seconds;
    int  theme_choice;      // 0 = Nord Dark, 1 = Midnight, 2 = Slate
    int  cursor_speed;
    int  brightness;
    int  governor_choice;   // 0 = powersave, 1 = ondemand, 2 = performance
} settings_data_t;

static const char* const settings_tabs[] = { "Appearance", "Input", "Power" };

static void settings_paint(gui_window_t* win) {
    settings_data_t* sd = (settings_data_t*)win->user_data;
    if (!sd) return;

    gui_widget_begin(win);

    int w = gui_widget_client_width();
    gui_tabbar(1, 6, 6, w - 12, settings_tabs, 3, &sd->active_tab);

    int y = 40;
    char buf[64];

    if (sd->active_tab == 0) {
        gui_label_bold(10, y, "Desktop Appearance", GUI_THEME_PRIMARY);
        y += 24;
        gui_checkbox(10, 10, y, "Show wallpaper grid overlay", &sd->show_grid);
        y += 22;
        gui_checkbox(11, 10, y, "Drop shadows under windows", &sd->window_shadows);
        y += 26;

        gui_label(10, y, "Theme", GUI_THEME_TEXT_MUTED);
        y += 14;
        gui_radio(12, 14, y, "Nord Dark",  &sd->theme_choice, 0); y += 18;
        gui_radio(13, 14, y, "Midnight",   &sd->theme_choice, 1); y += 18;
        gui_radio(14, 14, y, "Slate Grey", &sd->theme_choice, 2); y += 24;

        snprintf(buf, sizeof(buf), "Brightness: %d%%", sd->brightness);
        gui_label(10, y, buf, GUI_THEME_TEXT_MUTED);
        gui_slider(15, 150, y + 4, w - 170, &sd->brightness, 20, 100);

    } else if (sd->active_tab == 1) {
        gui_label_bold(10, y, "Input Devices", GUI_THEME_PRIMARY);
        y += 24;

        snprintf(buf, sizeof(buf), "Pointer speed: %d", sd->cursor_speed);
        gui_label(10, y, buf, GUI_THEME_TEXT_MUTED);
        gui_slider(20, 150, y + 4, w - 170, &sd->cursor_speed, 1, 10);
        y += 30;

        gui_checkbox(21, 10, y, "Enable UI sound effects", &sd->sound_enabled);
        y += 22;
        gui_checkbox(22, 10, y, "Show seconds in taskbar clock", &sd->show_clock_seconds);
        y += 30;

        const mouse_state_t* ms = mouse_get_state();
        snprintf(buf, sizeof(buf), "PS/2 mouse at (%d, %d)  L:%s R:%s",
                 ms ? ms->x : 0, ms ? ms->y : 0,
                 (ms && ms->left_btn) ? "down" : "up",
                 (ms && ms->right_btn) ? "down" : "up");
        gui_label(10, y, buf, GUI_THEME_TEXT_DIM);

    } else {
        gui_label_bold(10, y, "Power & Frequency", GUI_THEME_PRIMARY);
        y += 24;

        gui_label(10, y, "CPUFreq governor", GUI_THEME_TEXT_MUTED);
        y += 16;
        gui_radio(30, 14, y, "powersave",   &sd->governor_choice, 0); y += 18;
        gui_radio(31, 14, y, "ondemand",    &sd->governor_choice, 1); y += 18;
        gui_radio(32, 14, y, "performance", &sd->governor_choice, 2); y += 26;

        cpufreq_stats_t cf = cpufreq_get_stats();
        snprintf(buf, sizeof(buf), "Current clock: %u.%03u GHz",
                 cf.current_khz / 1000000, (cf.current_khz % 1000000) / 1000);
        gui_label(10, y, buf, GUI_THEME_TEXT_MAIN);
        y += 16;

        int load = (int)(cf.current_khz / 30000);
        if (load > 100) load = 100;
        gui_progress_bar(10, y, w - 24, 12, load, GUI_THEME_SUCCESS);
    }

    int btn_y = gui_widget_client_height() - 32;
    if (gui_button(90, 10, btn_y, 96, 24, "Apply")) {
        gui_dialog_show(GUI_DIALOG_INFO, "Settings",
                        "Preferences applied to the current desktop session.");
    }
    if (gui_button_colored(91, 114, btn_y, 96, 24, "Defaults", GUI_THEME_WARNING)) {
        sd->show_grid = true;
        sd->window_shadows = true;
        sd->sound_enabled = true;
        sd->show_clock_seconds = true;
        sd->theme_choice = 0;
        sd->cursor_speed = 5;
        sd->brightness = 100;
        sd->governor_choice = 1;
    }

    gui_widget_end();
}

void gui_app_settings_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Settings", x, y,
                                             (w > 0) ? w : 380, (h > 0) ? h : 300);
    if (!win) return;

    settings_data_t* sd = (settings_data_t*)kzalloc(sizeof(settings_data_t));
    if (!sd) return;

    sd->show_grid = true;
    sd->window_shadows = true;
    sd->sound_enabled = true;
    sd->show_clock_seconds = true;
    sd->cursor_speed = 5;
    sd->brightness = 100;
    sd->governor_choice = 1;

    win->user_data = sd;
    win->paint = settings_paint;
    win->handle_event = app_forward_event;
}

// ===========================================================================
// 2. Task Manager
// ===========================================================================
#define TASKMGR_MAX_ROWS 24

typedef struct {
    int  selected;
    int  row_count;
    char rows[TASKMGR_MAX_ROWS][40];
    const char* row_ptrs[TASKMGR_MAX_ROWS];
    pid_t pids[TASKMGR_MAX_ROWS];
    uint64_t last_refresh;
} taskmgr_data_t;

static const char* task_state_name(task_state_t s) {
    switch (s) {
        case TASK_STATE_READY:    return "READY";
        case TASK_STATE_RUNNING:  return "RUN  ";
        case TASK_STATE_BLOCKED:  return "BLOCK";
        case TASK_STATE_SLEEPING: return "SLEEP";
        case TASK_STATE_ZOMBIE:   return "ZOMBI";
        default:                  return "DEAD ";
    }
}

// Walk the low PID space; the kernel hands out small, dense pids.
static void taskmgr_refresh(taskmgr_data_t* td) {
    td->row_count = 0;

    for (pid_t pid = 0; pid < 64 && td->row_count < TASKMGR_MAX_ROWS; pid++) {
        task_t* t = task_find_by_pid(pid);
        if (!t) continue;

        snprintf(td->rows[td->row_count], sizeof(td->rows[0]),
                 "%4d %-16s %s p%d", t->pid, t->name,
                 task_state_name(t->state), t->priority);
        td->row_ptrs[td->row_count] = td->rows[td->row_count];
        td->pids[td->row_count] = t->pid;
        td->row_count++;
    }

    if (td->row_count == 0) {
        strcpy(td->rows[0], "   0 kernel_idle     RUN   p0");
        td->row_ptrs[0] = td->rows[0];
        td->pids[0] = 0;
        td->row_count = 1;
    }

    if (td->selected >= td->row_count) td->selected = td->row_count - 1;
    td->last_refresh = pit_get_ticks();
}

static void taskmgr_paint(gui_window_t* win) {
    taskmgr_data_t* td = (taskmgr_data_t*)win->user_data;
    if (!td) return;

    // Re-scan roughly twice a second rather than on every composited frame.
    if (pit_get_ticks() - td->last_refresh > 50) taskmgr_refresh(td);

    gui_widget_begin(win);

    int w = gui_widget_client_width();
    int h = gui_widget_client_height();
    char buf[64];

    gui_icon_draw(GUI_ICON_TASKS, win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + 7);
    gui_label_bold(30, 6, "Process Table", GUI_THEME_PRIMARY);

    gui_label(10, 28, " PID NAME             STATE PRIO", GUI_THEME_TEXT_DIM);
    gui_listbox(1, 10, 40, w - 20, h - 108, td->row_ptrs, td->row_count, &td->selected);

    int info_y = h - 62;
    gui_separator(10, info_y - 6, w - 20);

    uint64_t total = pmm_get_total_pages();
    uint64_t used  = pmm_get_used_pages();
    int mem_pct = total ? (int)((used * 100) / total) : 0;

    snprintf(buf, sizeof(buf), "Memory %llu / %llu MB",
             (unsigned long long)((used * 4096) / (1024 * 1024)),
             (unsigned long long)((total * 4096) / (1024 * 1024)));
    gui_label(10, info_y, buf, GUI_THEME_TEXT_MUTED);
    gui_progress_bar(160, info_y - 2, w - 176, 12, mem_pct,
                     mem_pct > 80 ? GUI_THEME_DANGER : GUI_THEME_SUCCESS);

    snprintf(buf, sizeof(buf), "Heap %llu KB used, %d task(s) live",
             (unsigned long long)(heap_get_used_bytes() / 1024), td->row_count);
    gui_label(10, info_y + 16, buf, GUI_THEME_TEXT_DIM);

    int btn_y = h - 30;
    if (gui_button(2, 10, btn_y, 84, 24, "Refresh")) {
        taskmgr_refresh(td);
    }
    if (gui_button_colored(3, 102, btn_y, 84, 24, "End Task", GUI_THEME_DANGER)) {
        pid_t victim = (td->selected < td->row_count) ? td->pids[td->selected] : -1;
        if (victim <= 0) {
            gui_dialog_show(GUI_DIALOG_WARNING, "Task Manager",
                            "PID 0 is the idle task and cannot be terminated.");
        } else {
            gui_dialog_show(GUI_DIALOG_WARNING, "Task Manager",
                            "Terminating kernel tasks from the desktop is disabled "
                            "in this build.");
        }
    }

    gui_widget_end();
}

void gui_app_taskmgr_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Task Manager", x, y,
                                             (w > 0) ? w : 360, (h > 0) ? h : 300);
    if (!win) return;

    taskmgr_data_t* td = (taskmgr_data_t*)kzalloc(sizeof(taskmgr_data_t));
    if (!td) return;

    taskmgr_refresh(td);
    win->user_data = td;
    win->paint = taskmgr_paint;
    win->handle_event = app_forward_event;
}

// ===========================================================================
// 3. Kernel Log Viewer
// ===========================================================================
#define LOG_MAX_LINES  200
#define LOG_LINE_CHARS 72

typedef struct {
    int  scroll;
    int  total_lines;
    bool wrap_long_lines;
    char lines[LOG_MAX_LINES][LOG_LINE_CHARS + 1];
} logview_data_t;

// The dmesg ring holds raw bytes with embedded log-level markers; split it
// into printable rows and drop the control prefixes.
static void logview_reload(logview_data_t* lv) {
    size_t size = 0;
    const char* buf = dmesg_get_buffer(&size);

    lv->total_lines = 0;
    if (!buf || size == 0) {
        strcpy(lv->lines[0], "(kernel ring buffer is empty)");
        lv->total_lines = 1;
        return;
    }

    size_t i = 0;
    while (i < size && lv->total_lines < LOG_MAX_LINES) {
        int col = 0;
        char* out = lv->lines[lv->total_lines];

        while (i < size && buf[i] != '\n') {
            char c = buf[i++];

            if (c == '\033') {
                // Skip an ANSI CSI sequence entirely.
                while (i < size && buf[i] != 'm' && buf[i] != '\n') i++;
                if (i < size && buf[i] == 'm') i++;
                continue;
            }
            if ((unsigned char)c < 32) continue;  // Log-level markers and tabs

            if (col < LOG_LINE_CHARS) out[col++] = c;
        }
        if (i < size) i++;  // Consume the newline

        out[col] = '\0';
        if (col > 0) lv->total_lines++;
    }

    if (lv->total_lines == 0) {
        strcpy(lv->lines[0], "(no printable log records)");
        lv->total_lines = 1;
    }
}

static void logview_paint(gui_window_t* win) {
    logview_data_t* lv = (logview_data_t*)win->user_data;
    if (!lv) return;

    gui_widget_begin(win);

    int w = gui_widget_client_width();
    int h = gui_widget_client_height();
    char buf[64];

    gui_icon_draw(GUI_ICON_LOG, win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + 7);
    gui_label_bold(30, 6, "Kernel Ring Buffer", GUI_THEME_PRIMARY);

    int view_y = 30;
    int view_h = h - 66;
    int row_h  = 11;
    int visible = view_h / row_h;

    gui_gfx_fill_rect(win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + view_y,
                      w - 32, view_h, 0xFF080B14);
    gui_gfx_draw_rect(win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + view_y,
                      w - 32, view_h, GUI_THEME_BORDER);

    if (lv->scroll > lv->total_lines - visible) lv->scroll = lv->total_lines - visible;
    if (lv->scroll < 0) lv->scroll = 0;

    for (int i = 0; i < visible && lv->scroll + i < lv->total_lines; i++) {
        const char* line = lv->lines[lv->scroll + i];

        // Colour-code by the severity words the kernel prints.
        uint32_t fg = GUI_THEME_TEXT_MUTED;
        if (strstr(line, "PANIC") || strstr(line, "ERROR")) fg = GUI_THEME_DANGER;
        else if (strstr(line, "WARN")) fg = GUI_THEME_WARNING;
        else if (strstr(line, "OK") || strstr(line, "online") || strstr(line, "initialized"))
            fg = GUI_THEME_SUCCESS;

        gui_label(14, view_y + 3 + i * row_h, line, fg);
    }

    gui_scrollbar(1, w - 21, view_y, view_h, &lv->scroll, lv->total_lines, visible);

    snprintf(buf, sizeof(buf), "%d lines, showing %d-%d",
             lv->total_lines, lv->scroll + 1,
             (lv->scroll + visible < lv->total_lines) ? lv->scroll + visible : lv->total_lines);
    gui_label(10, h - 30, buf, GUI_THEME_TEXT_DIM);

    if (gui_button(2, w - 190, h - 34, 84, 24, "Reload")) {
        logview_reload(lv);
        lv->scroll = 0;
    }
    if (gui_button(3, w - 100, h - 34, 84, 24, "End")) {
        lv->scroll = lv->total_lines - visible;
        if (lv->scroll < 0) lv->scroll = 0;
    }

    gui_widget_end();
}

void gui_app_logviewer_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Kernel Log", x, y,
                                             (w > 0) ? w : 620, (h > 0) ? h : 320);
    if (!win) return;

    logview_data_t* lv = (logview_data_t*)kzalloc(sizeof(logview_data_t));
    if (!lv) return;

    logview_reload(lv);
    win->user_data = lv;
    win->paint = logview_paint;
    win->handle_event = app_forward_event;
}

// ===========================================================================
// 4. KTest Runner
// ===========================================================================
typedef struct {
    int            selected_suite;
    bool           has_run;
    ktest_result_t result;
    char           suite_name[24];
} ktest_data_t;

static const char* const ktest_suite_names[] = {
    "(all suites)", "rbtree", "kfifo", "hashtable",
    "concurrency", "page_cache", "libcore"
};
#define KTEST_SUITE_COUNT 7

static void ktest_paint(gui_window_t* win) {
    ktest_data_t* kd = (ktest_data_t*)win->user_data;
    if (!kd) return;

    gui_widget_begin(win);

    int w = gui_widget_client_width();
    int h = gui_widget_client_height();
    char buf[64];

    gui_icon_draw(GUI_ICON_FLASK, win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + 7);
    gui_label_bold(30, 6, "In-Kernel Test Harness", GUI_THEME_PRIMARY);

    gui_label(10, 30, "Suite", GUI_THEME_TEXT_DIM);
    gui_listbox(1, 10, 42, 150, 108, ktest_suite_names, KTEST_SUITE_COUNT,
                &kd->selected_suite);

    if (gui_button_colored(2, 10, 156, 150, 26, "Run Tests", GUI_THEME_SUCCESS)) {
        // Suite 0 is the "all" pseudo-entry; the rest map 1:1 by name.
        if (kd->selected_suite == 0) {
            ktest_run_all(&kd->result);
            strcpy(kd->suite_name, "all suites");
        } else {
            ktest_run_suite(ktest_suite_names[kd->selected_suite], &kd->result);
            strncpy(kd->suite_name, ktest_suite_names[kd->selected_suite],
                    sizeof(kd->suite_name) - 1);
        }
        kd->has_run = true;
    }

    int rx = 174;
    gui_panel(rx, 42, w - rx - 14, 140, "Results");

    if (!kd->has_run) {
        gui_label(rx + 12, 66, "No run yet.", GUI_THEME_TEXT_DIM);
        gui_label(rx + 12, 82, "Pick a suite and press", GUI_THEME_TEXT_DIM);
        gui_label(rx + 12, 94, "Run Tests. Full per-case", GUI_THEME_TEXT_DIM);
        gui_label(rx + 12, 106, "output goes to the serial", GUI_THEME_TEXT_DIM);
        gui_label(rx + 12, 118, "console and dmesg.", GUI_THEME_TEXT_DIM);
    } else {
        const ktest_result_t* r = &kd->result;
        bool green = (r->tests_failed == 0);

        snprintf(buf, sizeof(buf), "Suite: %s", kd->suite_name);
        gui_label(rx + 12, 56, buf, GUI_THEME_TEXT_MAIN);

        snprintf(buf, sizeof(buf), "%u tests, %u passed, %u failed",
                 r->tests_run, r->tests_passed, r->tests_failed);
        gui_label(rx + 12, 72, buf, GUI_THEME_TEXT_MUTED);

        snprintf(buf, sizeof(buf), "%u assertions, %u failures",
                 r->assertions, r->assertion_failures);
        gui_label(rx + 12, 86, buf, GUI_THEME_TEXT_MUTED);

        snprintf(buf, sizeof(buf), "Elapsed: %llu ms",
                 (unsigned long long)(r->elapsed_ticks * 10));
        gui_label(rx + 12, 100, buf, GUI_THEME_TEXT_DIM);

        int pct = r->tests_run ? (int)((r->tests_passed * 100) / r->tests_run) : 0;
        gui_progress_bar(rx + 12, 118, w - rx - 40, 14, pct,
                         green ? GUI_THEME_SUCCESS : GUI_THEME_DANGER);

        gui_badge(rx + 12, 140, green ? "ALL PASSED" : "FAILURES",
                  green ? GUI_THEME_SUCCESS : GUI_THEME_DANGER, GUI_COLOR_BLACK);
    }

    gui_separator(10, h - 42, w - 20);
    gui_label(10, h - 34, "Suites cover rbtree, kfifo, hashtable, RCU,",
              GUI_THEME_TEXT_DIM);
    gui_label(10, h - 22, "futex, wait queues, page cache and libcore.",
              GUI_THEME_TEXT_DIM);

    gui_widget_end();
}

void gui_app_ktest_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("KTest Runner", x, y,
                                             (w > 0) ? w : 460, (h > 0) ? h : 250);
    if (!win) return;

    ktest_data_t* kd = (ktest_data_t*)kzalloc(sizeof(ktest_data_t));
    if (!kd) return;

    win->user_data = kd;
    win->paint = ktest_paint;
    win->handle_event = app_forward_event;
}

// ===========================================================================
// 5. Clock & Calendar
// ===========================================================================
typedef struct {
    bool show_seconds;
    int  view_month;   // 1-12; 0 means "follow the RTC"
} clock_data_t;

static const char* const month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static int days_in_month(int month, int year) {
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 30;
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
    return days[month - 1];
}

// Zeller's congruence, returning 0 = Sunday.
static int day_of_week(int day, int month, int year) {
    if (month < 3) { month += 12; year -= 1; }
    int k = year % 100;
    int j = year / 100;
    int hh = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (hh + 6) % 7;
}

static void clock_paint(gui_window_t* win) {
    clock_data_t* cd = (clock_data_t*)win->user_data;
    if (!cd) return;

    rtc_time_t now;
    rtc_get_time(&now);

    gui_widget_begin(win);
    int w = gui_widget_client_width();
    char buf[64];

    int month = cd->view_month ? cd->view_month : (int)now.month;
    int year  = (int)now.year;

    // Big digital readout
    if (cd->show_seconds) {
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", now.hour, now.minute, now.second);
    } else {
        snprintf(buf, sizeof(buf), "%02u:%02u", now.hour, now.minute);
    }

    int tw = (int)strlen(buf) * 8;
    gui_gfx_draw_string_16_shadow(win->x + 1 + (w - tw) / 2,
                                  win->y + GUI_TITLEBAR_HEIGHT + 9,
                                  buf, GUI_THEME_PRIMARY, GUI_THEME_PRIMARY_DARK);

    snprintf(buf, sizeof(buf), "%s %u, %d", month_names[month - 1], now.day, year);
    gui_label_aligned(0, 32, w, buf, GUI_THEME_TEXT_MAIN, GUI_ALIGN_CENTER);

    // Month grid
    int gx = 12, gy = 50;
    int cell_w = (w - 24) / 7;
    int cell_h = 16;

    const char* const dows[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    for (int i = 0; i < 7; i++) {
        gui_label(gx + i * cell_w + (cell_w - 16) / 2, gy, dows[i],
                  (i == 0 || i == 6) ? GUI_THEME_DANGER : GUI_THEME_TEXT_DIM);
    }
    gui_separator(gx, gy + 12, w - 24);

    int first_dow = day_of_week(1, month, year);
    int total_days = days_in_month(month, year);

    for (int d = 1; d <= total_days; d++) {
        int slot = first_dow + d - 1;
        int col = slot % 7;
        int row = slot / 7;
        int cx = gx + col * cell_w;
        int cy = gy + 18 + row * cell_h;

        bool today = (d == (int)now.day && month == (int)now.month);
        if (today) {
            gui_gfx_fill_rect(win->x + 1 + cx, win->y + GUI_TITLEBAR_HEIGHT + 1 + cy - 2,
                              cell_w - 2, cell_h - 2, GUI_THEME_PRIMARY_DARK);
        }

        snprintf(buf, sizeof(buf), "%2d", d);
        gui_label(cx + (cell_w - 16) / 2, cy, buf,
                  today ? GUI_COLOR_WHITE
                        : (col == 0 || col == 6) ? GUI_THEME_TEXT_DIM
                                                 : GUI_THEME_TEXT_MUTED);
    }

    int h = gui_widget_client_height();
    if (gui_button(1, 12, h - 30, 30, 22, "<")) {
        cd->view_month = (month == 1) ? 12 : month - 1;
    }
    if (gui_button(2, 46, h - 30, 30, 22, ">")) {
        cd->view_month = (month == 12) ? 1 : month + 1;
    }
    if (gui_button(3, 80, h - 30, 62, 22, "Today")) {
        cd->view_month = 0;
    }
    gui_checkbox(4, 152, h - 28, "Seconds", &cd->show_seconds);

    gui_widget_end();
}

void gui_app_clock_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Clock & Calendar", x, y,
                                             (w > 0) ? w : 270, (h > 0) ? h : 250);
    if (!win) return;

    clock_data_t* cd = (clock_data_t*)kzalloc(sizeof(clock_data_t));
    if (!cd) return;

    cd->show_seconds = true;
    win->user_data = cd;
    win->paint = clock_paint;
    win->handle_event = app_forward_event;
}

// ===========================================================================
// 6. Text Editor
// ===========================================================================
#define EDITOR_ROWS       14
#define EDITOR_ROW_CHARS  56

typedef struct {
    char lines[EDITOR_ROWS][EDITOR_ROW_CHARS + 1];
    int  line_count;
    int  cursor_line;
    char filename[48];
    bool modified;
    bool word_wrap;
} editor_data_t;

static void editor_insert_char(editor_data_t* ed, char c) {
    if (ed->cursor_line >= EDITOR_ROWS) return;

    char* line = ed->lines[ed->cursor_line];
    size_t len = strlen(line);

    if (c == '\n' || c == '\r') {
        if (ed->line_count < EDITOR_ROWS) {
            // Push the following lines down to make room for the new one.
            for (int i = ed->line_count; i > ed->cursor_line + 1; i--) {
                strcpy(ed->lines[i], ed->lines[i - 1]);
            }
            ed->line_count++;
            ed->cursor_line++;
            ed->lines[ed->cursor_line][0] = '\0';
            ed->modified = true;
        }
        return;
    }

    if (c == '\b' || (uint8_t)c == 0x7F) {
        if (len > 0) {
            line[len - 1] = '\0';
            ed->modified = true;
        } else if (ed->cursor_line > 0) {
            ed->cursor_line--;
        }
        return;
    }

    if (c >= 32 && c <= 126 && len < EDITOR_ROW_CHARS) {
        line[len] = c;
        line[len + 1] = '\0';
        ed->modified = true;
    }
}

static void editor_event(gui_window_t* win, const gui_event_t* ev) {
    editor_data_t* ed = (editor_data_t*)win->user_data;

    if (ed && ev->type == GUI_EVENT_KEY_DOWN) {
        uint16_t key = ev->key;
        if (key == KEY_UP) {
            if (ed->cursor_line > 0) ed->cursor_line--;
            return;
        }
        if (key == KEY_DOWN) {
            if (ed->cursor_line < ed->line_count - 1) ed->cursor_line++;
            return;
        }
        if (!(key & KEY_SPECIAL_FLAG)) {
            editor_insert_char(ed, (char)(key & 0xFF));
            return;
        }
    }

    app_forward_event(win, ev);
}

static void editor_paint(gui_window_t* win) {
    editor_data_t* ed = (editor_data_t*)win->user_data;
    if (!ed) return;

    gui_widget_begin(win);

    int w = gui_widget_client_width();
    int h = gui_widget_client_height();
    char buf[80];

    // Toolbar
    gui_icon_draw(GUI_ICON_FILE, win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + 6);
    snprintf(buf, sizeof(buf), "%s%s", ed->filename, ed->modified ? " *" : "");
    gui_label(30, 10, buf, ed->modified ? GUI_THEME_WARNING : GUI_THEME_TEXT_MAIN);

    if (gui_button(1, w - 176, 6, 52, 20, "New")) {
        memset(ed->lines, 0, sizeof(ed->lines));
        ed->line_count = 1;
        ed->cursor_line = 0;
        ed->modified = false;
        strcpy(ed->filename, "untitled.txt");
    }
    if (gui_button(2, w - 120, 6, 52, 20, "Save")) {
        ed->modified = false;
        gui_dialog_show(GUI_DIALOG_INFO, "Text Editor",
                        "This build keeps the buffer in memory only; VFS write-back "
                        "is not wired up yet.");
    }
    if (gui_button(3, w - 64, 6, 52, 20, "Wrap")) {
        ed->word_wrap = !ed->word_wrap;
    }

    // Text area with a gutter
    int area_y = 32;
    int area_h = h - area_y - 26;
    int row_h  = 14;

    gui_gfx_fill_rect(win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + area_y,
                      w - 18, area_h, 0xFF080B14);
    gui_gfx_draw_rect(win->x + 9, win->y + GUI_TITLEBAR_HEIGHT + area_y,
                      w - 18, area_h, GUI_THEME_BORDER);
    gui_gfx_draw_line(win->x + 40, win->y + GUI_TITLEBAR_HEIGHT + area_y + 1,
                      win->x + 40, win->y + GUI_TITLEBAR_HEIGHT + area_y + area_h - 2,
                      GUI_THEME_BORDER);

    int visible = (area_h - 6) / row_h;
    for (int i = 0; i < visible && i < ed->line_count; i++) {
        int ry = area_y + 4 + i * row_h;
        bool is_cursor = (i == ed->cursor_line);

        if (is_cursor) {
            gui_gfx_fill_rect(win->x + 42, win->y + GUI_TITLEBAR_HEIGHT + ry - 2,
                              w - 54, row_h, 0xFF11182B);
        }

        snprintf(buf, sizeof(buf), "%3d", i + 1);
        gui_label(14, ry, buf, is_cursor ? GUI_THEME_PRIMARY : GUI_THEME_TEXT_DIM);
        gui_label(38, ry, ed->lines[i], GUI_THEME_TEXT_MAIN);

        if (is_cursor) {
            int cx = 38 + (int)strlen(ed->lines[i]) * 8;
            gui_gfx_fill_rect(win->x + 1 + cx, win->y + GUI_TITLEBAR_HEIGHT + 1 + ry - 1,
                              7, 10, GUI_THEME_SUCCESS);
        }
    }

    snprintf(buf, sizeof(buf), "Ln %d/%d  Col %d  %s",
             ed->cursor_line + 1, ed->line_count,
             (int)strlen(ed->lines[ed->cursor_line]) + 1,
             ed->word_wrap ? "WRAP" : "NOWRAP");
    gui_label(10, h - 16, buf, GUI_THEME_TEXT_DIM);

    gui_widget_end();
}

void gui_app_editor_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Text Editor", x, y,
                                             (w > 0) ? w : 500, (h > 0) ? h : 320);
    if (!win) return;

    editor_data_t* ed = (editor_data_t*)kzalloc(sizeof(editor_data_t));
    if (!ed) return;

    strcpy(ed->filename, "untitled.txt");
    strcpy(ed->lines[0], "# SUB-OS Text Editor");
    strcpy(ed->lines[1], "");
    strcpy(ed->lines[2], "Type to edit. Enter adds a line,");
    strcpy(ed->lines[3], "Up/Down move between lines.");
    ed->line_count = 4;
    ed->cursor_line = 3;

    win->user_data = ed;
    win->paint = editor_paint;
    win->handle_event = editor_event;
}
