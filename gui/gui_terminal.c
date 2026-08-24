// SUB-OS Desktop Terminal Emulator
//
// Commands typed here run through the same LazyBox applet table the kernel TTY
// uses. LazyBox reports everything via printk, so before dispatching an applet
// the terminal installs a printk sink; the applet's output is streamed into
// the window's scrollback instead of the text console, ANSI colour codes are
// translated to per-line colours, and the sink is removed afterwards.

#include <gui/gui_terminal.h>
#include <gui/gui_widgets.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <userland/lazybox.h>
#include <userland/shell.h>
#include <kernel/printk.h>
#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <drivers/keyboard.h>
#include <lib/string.h>
#include <lib/printf.h>

#define TERM_SCROLLBACK   256
#define TERM_LINE_CHARS   118
#define TERM_INPUT_MAX    120
#define TERM_HISTORY      16
#define TERM_ROW_H        12
#define TERM_ARGV_MAX     16

typedef struct {
    char     text[TERM_LINE_CHARS + 1];
    uint32_t color;
} term_line_t;

typedef struct {
    term_line_t lines[TERM_SCROLLBACK];
    int         line_count;       // Total lines ever written, capped at TERM_SCROLLBACK
    int         scroll;           // First visible line
    bool        follow_tail;      // Auto-scroll while new output arrives

    char        input[TERM_INPUT_MAX + 1];
    int         input_len;

    char        history[TERM_HISTORY][TERM_INPUT_MAX + 1];
    int         history_count;
    int         history_pos;      // -1 when editing a fresh line

    // Partial line being assembled from sink writes
    char        pending[TERM_LINE_CHARS + 1];
    int         pending_len;
    uint32_t    pending_color;
    bool        busy;
} term_data_t;

// ---------------------------------------------------------------------------
// Scrollback
// ---------------------------------------------------------------------------

static void term_scroll_to_tail(term_data_t* td, int visible_rows) {
    td->scroll = td->line_count - visible_rows;
    if (td->scroll < 0) td->scroll = 0;
}

static void term_push_line(term_data_t* td, const char* text, uint32_t color) {
    if (td->line_count == TERM_SCROLLBACK) {
        // Full: drop the oldest line and shift the window up by one.
        for (int i = 0; i < TERM_SCROLLBACK - 1; i++) {
            td->lines[i] = td->lines[i + 1];
        }
        td->line_count--;
        if (td->scroll > 0) td->scroll--;
    }

    term_line_t* line = &td->lines[td->line_count++];
    strncpy(line->text, text ? text : "", TERM_LINE_CHARS);
    line->text[TERM_LINE_CHARS] = '\0';
    line->color = color;
}

static void term_flush_pending(term_data_t* td) {
    td->pending[td->pending_len] = '\0';
    term_push_line(td, td->pending, td->pending_color);
    td->pending_len = 0;
    td->pending[0] = '\0';
}

static void term_printf(term_data_t* td, uint32_t color, const char* fmt, ...) {
    char buf[TERM_LINE_CHARS + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    term_push_line(td, buf, color);
}

// ---------------------------------------------------------------------------
// ANSI translation
// ---------------------------------------------------------------------------

// Map an SGR parameter to the palette. Only the colour attributes LazyBox
// actually emits are handled; everything else keeps the current colour.
static uint32_t sgr_to_color(int code, uint32_t current) {
    switch (code) {
        case 0:  return GUI_THEME_TEXT_MAIN;   // reset
        case 30: case 90:  return GUI_THEME_TEXT_DIM;
        case 31: case 91:  return GUI_THEME_DANGER;
        case 32: case 92:  return GUI_THEME_SUCCESS;
        case 33: case 93:  return GUI_THEME_WARNING;
        case 34: case 94:  return GUI_THEME_ACCENT;
        case 35: case 95:  return 0xFFEC4899;  // magenta
        case 36: case 96:  return GUI_THEME_PRIMARY;
        case 37: case 97:  return GUI_THEME_TEXT_MAIN;
        default: return current;
    }
}

// CP437 fallback.
//
// The VGA text console renders code page 437 natively, so LazyBox draws boxes
// and colour swatches with high-ASCII bytes such as 0xDB (full block). The GUI
// font only covers printable ASCII, so translate the shapes those applets
// actually use rather than dropping them to '?'. Returns 0 to skip the byte.
static char cp437_to_ascii(unsigned char c) {
    switch (c) {
        case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF: return '#'; // blocks
        case 0xFE: case 0xFD: return '#';                                  // small squares
        case 0xB2: return '#';                                             // dark shade
        case 0xB1: return ':';                                             // medium shade
        case 0xB0: return '.';                                             // light shade
        case 0xC4: case 0xCD: return '-';                                  // horizontal rules
        case 0xB3: case 0xBA: return '|';                                  // vertical rules
        case 0xDA: case 0xBF: case 0xC0: case 0xD9:                        // single corners
        case 0xC9: case 0xBB: case 0xC8: case 0xBC:                        // double corners
        case 0xC3: case 0xB4: case 0xC2: case 0xC1: case 0xC5:             // tees and cross
            return '+';
        case 0x07: case 0xF9: case 0xFA: return '.';                       // bullets
        case 0x1A: case 0x10: return '>';                                  // arrows
        case 0x1B: case 0x11: return '<';
        case 0x18: case 0x1E: return '^';
        case 0x19: case 0x1F: return 'v';
        case 0xF8: return 'o';                                             // degree
        default:   return 0;                                               // drop
    }
}

// Consume captured bytes, splitting on newlines and folding ANSI escapes into
// the colour of the line being assembled.
static void term_absorb(term_data_t* td, const char* text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = text[i];

        if (c == '\033') {
            // CSI sequence: ESC [ params letter
            size_t j = i + 1;
            if (j < len && text[j] == '[') {
                j++;
                int value = 0;
                bool have_value = false;

                while (j < len) {
                    char p = text[j];
                    if (p >= '0' && p <= '9') {
                        value = value * 10 + (p - '0');
                        have_value = true;
                        j++;
                    } else if (p == ';') {
                        if (have_value) td->pending_color = sgr_to_color(value, td->pending_color);
                        value = 0;
                        have_value = false;
                        j++;
                    } else {
                        if (p == 'm') {
                            td->pending_color = sgr_to_color(have_value ? value : 0,
                                                             td->pending_color);
                        }
                        j++;
                        break;
                    }
                }
                i = j - 1;
            }
            continue;
        }

        if (c == '\n') {
            term_flush_pending(td);
            continue;
        }
        if (c == '\r') {
            td->pending_len = 0;
            continue;
        }
        if (c == '\t') {
            // Expand to the next 8-column stop.
            int stop = (td->pending_len + 8) & ~7;
            while (td->pending_len < stop && td->pending_len < TERM_LINE_CHARS) {
                td->pending[td->pending_len++] = ' ';
            }
            continue;
        }
        unsigned char uc = (unsigned char)c;
        if (uc < 32 || uc > 126) {
            char sub = cp437_to_ascii(uc);
            if (!sub) continue;   // Control byte or a glyph with no ASCII stand-in
            c = sub;
        }

        if (td->pending_len >= TERM_LINE_CHARS) term_flush_pending(td);
        td->pending[td->pending_len++] = c;
    }
}

static void term_sink(const char* text, size_t len, void* ctx) {
    term_absorb((term_data_t*)ctx, text, len);
}

// ---------------------------------------------------------------------------
// Command execution
// ---------------------------------------------------------------------------

// Applets that read the keyboard directly would block the compositor, since
// the desktop drives input from its own frame loop.
static bool applet_is_interactive(const char* name) {
    return strcmp(name, "nano") == 0 || strcmp(name, "snake") == 0 ||
           strcmp(name, "sh") == 0   || strcmp(name, "top") == 0;
}

static void term_run_command(term_data_t* td, const char* cmd) {
    char work[TERM_INPUT_MAX + 1];
    strncpy(work, cmd, TERM_INPUT_MAX);
    work[TERM_INPUT_MAX] = '\0';

    // Same quote-aware tokenizer the TTY shell uses.
    char* argv[TERM_ARGV_MAX];
    int   argc = 0;
    char* token = work;

    while (*token) {
        while (*token == ' ') *token++ = '\0';
        if (*token == '\0') break;

        if (*token == '"' || *token == '\'') {
            char quote = *token++;
            if (argc < TERM_ARGV_MAX) argv[argc++] = token;
            while (*token && *token != quote) token++;
            if (*token == quote) *token++ = '\0';
        } else {
            if (argc < TERM_ARGV_MAX) argv[argc++] = token;
            while (*token && *token != ' ') token++;
        }
    }
    if (argc == 0) return;

    // Terminal built-ins first.
    if (strcmp(argv[0], "clear") == 0 || strcmp(argv[0], "cls") == 0) {
        td->line_count = 0;
        td->scroll = 0;
        return;
    }
    if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
        term_printf(td, GUI_THEME_TEXT_DIM, "Close the window to end this session.");
        return;
    }

    bool is_applet  = lazybox_has_applet(argv[0]);
    bool is_builtin = shell_is_builtin(argv[0]);

    if (!is_applet && !is_builtin) {
        term_printf(td, GUI_THEME_DANGER,
                    "%s: command not found. Try 'lazybox' for the applet list.", argv[0]);
        return;
    }

    if (is_applet && applet_is_interactive(argv[0])) {
        term_printf(td, GUI_THEME_WARNING,
                    "%s reads the keyboard directly and would freeze the desktop.",
                    argv[0]);
        term_printf(td, GUI_THEME_TEXT_DIM,
                    "Run it from the kernel TTY instead (Esc leaves the desktop).");
        return;
    }

    // Capture printk output into this window's scrollback for the duration of
    // the command. Applets take precedence over builtins, matching the TTY.
    td->busy = true;
    td->pending_len = 0;
    td->pending_color = GUI_THEME_TEXT_MAIN;

    printk_set_sink(term_sink, td);
    if (is_applet) {
        lazybox_run_applet(argv[0], argc, argv);
    } else {
        shell_execute_builtin(cmd, argc, argv, false);
    }
    printk_clear_sink();

    if (td->pending_len > 0) term_flush_pending(td);
    td->busy = false;
}

static void term_submit(term_data_t* td) {
    const char* cwd = vfs_getcwd();

    term_printf(td, GUI_THEME_SUCCESS, "%s $ %s", cwd ? cwd : "/", td->input);

    if (td->input_len > 0) {
        // Record in history, skipping an immediate repeat.
        bool repeat = (td->history_count > 0 &&
                       strcmp(td->history[td->history_count - 1], td->input) == 0);
        if (!repeat) {
            if (td->history_count == TERM_HISTORY) {
                for (int i = 0; i < TERM_HISTORY - 1; i++) {
                    strcpy(td->history[i], td->history[i + 1]);
                }
                td->history_count--;
            }
            strcpy(td->history[td->history_count++], td->input);
        }

        term_run_command(td, td->input);
    }

    td->input[0] = '\0';
    td->input_len = 0;
    td->history_pos = -1;
    td->follow_tail = true;
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------

static void term_recall_history(term_data_t* td, int direction) {
    if (td->history_count == 0) return;

    if (td->history_pos == -1) {
        td->history_pos = (direction < 0) ? td->history_count - 1 : -1;
    } else {
        td->history_pos += (direction < 0) ? -1 : 1;
    }

    if (td->history_pos < 0) td->history_pos = 0;
    if (td->history_pos >= td->history_count) {
        // Past the newest entry: back to an empty prompt.
        td->history_pos = -1;
        td->input[0] = '\0';
        td->input_len = 0;
        return;
    }

    strncpy(td->input, td->history[td->history_pos], TERM_INPUT_MAX);
    td->input[TERM_INPUT_MAX] = '\0';
    td->input_len = (int)strlen(td->input);
}

static void terminal_event(gui_window_t* win, const gui_event_t* ev) {
    term_data_t* td = (term_data_t*)win->user_data;
    if (!td) return;

    if (ev->type == GUI_EVENT_CLOSE) {
        // A sink pointing at freed memory would corrupt the next printk.
        if (td->busy) printk_clear_sink();
        kfree(td);
        win->user_data = NULL;
        return;
    }

    if (ev->type == GUI_EVENT_KEY_DOWN) {
        uint16_t key = ev->key;
        int visible = (win->height - GUI_TITLEBAR_HEIGHT - 40) / TERM_ROW_H;
        if (visible < 1) visible = 1;

        if (key & KEY_SPECIAL_FLAG) {
            switch (key) {
                case KEY_UP:        term_recall_history(td, -1); break;
                case KEY_DOWN:      term_recall_history(td, +1); break;
                case KEY_PAGE_UP:
                    td->follow_tail = false;
                    td->scroll -= visible;
                    if (td->scroll < 0) td->scroll = 0;
                    break;
                case KEY_PAGE_DOWN:
                    td->scroll += visible;
                    term_scroll_to_tail(td, visible);
                    td->follow_tail = true;
                    break;
                case KEY_HOME:
                    td->follow_tail = false;
                    td->scroll = 0;
                    break;
                case KEY_END:
                    td->follow_tail = true;
                    term_scroll_to_tail(td, visible);
                    break;
                default: break;
            }
            return;
        }

        char c = (char)(key & 0xFF);
        if (c == '\n' || c == '\r') {
            term_submit(td);
        } else if (c == '\b' || (uint8_t)c == 0x7F) {
            if (td->input_len > 0) td->input[--td->input_len] = '\0';
        } else if (c >= 32 && c <= 126 && td->input_len < TERM_INPUT_MAX) {
            td->input[td->input_len++] = c;
            td->input[td->input_len] = '\0';
        }
        return;
    }

    gui_widget_feed_event(win, ev);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

static void terminal_paint(gui_window_t* win) {
    term_data_t* td = (term_data_t*)win->user_data;
    if (!td) return;

    int cx = win->x + 1;
    int cy = win->y + GUI_TITLEBAR_HEIGHT + 1;
    int cw = win->width - 2;
    int ch = win->height - GUI_TITLEBAR_HEIGHT - 2;

    gui_gfx_fill_rect(cx, cy, cw, ch, 0xFF080B14);

    int pad        = 6;
    int prompt_h   = 18;
    int status_h   = 12;
    int view_h     = ch - prompt_h - status_h - pad;
    int visible    = view_h / TERM_ROW_H;
    if (visible < 1) visible = 1;

    int max_cols = (cw - pad * 2) / 8;
    if (max_cols < 1) max_cols = 1;
    if (max_cols > TERM_LINE_CHARS) max_cols = TERM_LINE_CHARS;

    if (td->follow_tail) term_scroll_to_tail(td, visible);
    if (td->scroll > td->line_count - visible) term_scroll_to_tail(td, visible);
    if (td->scroll < 0) td->scroll = 0;

    // Scrollback
    for (int row = 0; row < visible; row++) {
        int idx = td->scroll + row;
        if (idx >= td->line_count) break;

        char clipped[TERM_LINE_CHARS + 1];
        strncpy(clipped, td->lines[idx].text, (size_t)max_cols);
        clipped[max_cols] = '\0';

        gui_gfx_draw_string(cx + pad, cy + pad + row * TERM_ROW_H, clipped,
                            td->lines[idx].color);
    }

    // Prompt
    int prompt_y = cy + ch - prompt_h - status_h;
    gui_gfx_draw_line(cx + 1, prompt_y - 3, cx + cw - 2, prompt_y - 3, GUI_THEME_BORDER);

    const char* cwd = vfs_getcwd();
    char prompt[40];
    snprintf(prompt, sizeof(prompt), "%s $ ", cwd ? cwd : "/");
    int prompt_w = (int)strlen(prompt) * 8;

    gui_gfx_draw_string(cx + pad, prompt_y + 2, prompt, GUI_THEME_PRIMARY);

    // Show the tail of a long command line so the cursor stays visible.
    int room = max_cols - (int)strlen(prompt) - 1;
    if (room < 1) room = 1;
    const char* view = (td->input_len > room) ? td->input + (td->input_len - room)
                                              : td->input;
    gui_gfx_draw_string(cx + pad + prompt_w, prompt_y + 2, view, GUI_THEME_TEXT_MAIN);

    if (win->active) {
        int cursor_x = cx + pad + prompt_w + (int)strlen(view) * 8;
        gui_gfx_fill_rect(cursor_x, prompt_y + 1, 8, 10, GUI_THEME_SUCCESS);
    }

    // Status line
    char status[72];
    if (td->line_count > visible && !td->follow_tail) {
        snprintf(status, sizeof(status), "%d lines  showing %d-%d  [End] to follow",
                 td->line_count, td->scroll + 1,
                 (td->scroll + visible < td->line_count) ? td->scroll + visible
                                                         : td->line_count);
    } else {
        snprintf(status, sizeof(status), "%d lines  Up/Down history  PgUp/PgDn scroll",
                 td->line_count);
    }
    gui_gfx_draw_string(cx + pad, cy + ch - status_h, status, GUI_THEME_TEXT_DIM);
}

void gui_app_terminal_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Terminal", x, y,
                                             (w > 0) ? w : 520, (h > 0) ? h : 320);
    if (!win) return;

    term_data_t* td = (term_data_t*)kzalloc(sizeof(term_data_t));
    if (!td) return;

    td->history_pos   = -1;
    td->follow_tail   = true;
    td->pending_color = GUI_THEME_TEXT_MAIN;

    term_printf(td, GUI_THEME_PRIMARY, "SUB-OS Terminal - LazyBox %s", LAZYBOX_VERSION);
    term_printf(td, GUI_THEME_TEXT_MUTED,
                "LazyBox applets and shell builtins both run here. Try 'ls',");
    term_printf(td, GUI_THEME_TEXT_MUTED,
                "'uname -a', 'free', 'neofetch', 'lspci', 'ktest', 'help'.");
    term_printf(td, GUI_THEME_TEXT_DIM, "");

    win->user_data    = td;
    win->paint        = terminal_paint;
    win->handle_event = terminal_event;
}
