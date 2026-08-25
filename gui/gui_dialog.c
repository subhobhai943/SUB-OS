// Modal dialog layer for the SUB-OS desktop
#include <gui/gui_dialog.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <gui/gui_icons.h>
#include <drivers/keyboard.h>
#include <lib/string.h>

#define DIALOG_W 300
#define DIALOG_H 132
#define DIALOG_TITLEBAR 24
#define DIALOG_BTN_W 84
#define DIALOG_BTN_H 24
#define DIALOG_MAX_LINES 4
#define DIALOG_LINE_CHARS 28

typedef struct {
    bool              open;
    gui_dialog_kind_t kind;
    char              title[48];
    char              lines[DIALOG_MAX_LINES][DIALOG_LINE_CHARS + 1];
    int               line_count;
    bool              has_cancel;
    gui_dialog_cb_t   callback;
    void*             ctx;
    int               hover_button;   // 0 = none, 1 = OK, 2 = Cancel
} dialog_state_t;

static dialog_state_t g_dlg;

void gui_dialog_init(void) {
    memset(&g_dlg, 0, sizeof(g_dlg));
}

// Break the message on spaces so words are not split across rows.
static void wrap_message(const char* message) {
    g_dlg.line_count = 0;
    if (!message) return;

    const char* p = message;
    while (*p && g_dlg.line_count < DIALOG_MAX_LINES) {
        int take = 0;
        int last_space = -1;

        while (p[take] && take < DIALOG_LINE_CHARS) {
            if (p[take] == ' ') last_space = take;
            take++;
        }

        if (p[take] && last_space > 0) take = last_space;

        int n = take;
        if (n > DIALOG_LINE_CHARS) n = DIALOG_LINE_CHARS;
        memcpy(g_dlg.lines[g_dlg.line_count], p, (size_t)n);
        g_dlg.lines[g_dlg.line_count][n] = '\0';
        g_dlg.line_count++;

        p += take;
        while (*p == ' ') p++;
    }
}

static void open_dialog(gui_dialog_kind_t kind, const char* title, const char* message,
                        bool has_cancel, gui_dialog_cb_t cb, void* ctx) {
    memset(&g_dlg, 0, sizeof(g_dlg));
    g_dlg.open = true;
    g_dlg.kind = kind;
    g_dlg.has_cancel = has_cancel;
    g_dlg.callback = cb;
    g_dlg.ctx = ctx;

    strncpy(g_dlg.title, title ? title : "SUB-OS", sizeof(g_dlg.title) - 1);
    wrap_message(message);
}

void gui_dialog_show(gui_dialog_kind_t kind, const char* title, const char* message) {
    open_dialog(kind, title, message, false, NULL, NULL);
}

void gui_dialog_confirm(const char* title, const char* message,
                        gui_dialog_cb_t on_result, void* ctx) {
    open_dialog(GUI_DIALOG_CONFIRM, title, message, true, on_result, ctx);
}

void gui_dialog_dismiss(void) {
    g_dlg.open = false;
}

bool gui_dialog_is_open(void) {
    return g_dlg.open;
}

static void finish(gui_dialog_result_t result) {
    gui_dialog_cb_t cb = g_dlg.callback;
    void* ctx = g_dlg.ctx;

    // Clear before invoking so a callback may immediately open another dialog.
    g_dlg.open = false;
    g_dlg.callback = NULL;

    if (cb) cb(result, ctx);
}

static void dialog_geometry(int* dx, int* dy) {
    *dx = (gui_gfx_get_width() - DIALOG_W) / 2;
    *dy = (gui_gfx_get_height() - DIALOG_H) / 2 - 20;
}

bool gui_dialog_handle_mouse(int mx, int my, bool clicked) {
    if (!g_dlg.open) return false;

    int dx, dy;
    dialog_geometry(&dx, &dy);

    int btn_y = dy + DIALOG_H - DIALOG_BTN_H - 12;
    int ok_x, cancel_x;
    if (g_dlg.has_cancel) {
        ok_x     = dx + DIALOG_W - (DIALOG_BTN_W * 2) - 22;
        cancel_x = dx + DIALOG_W - DIALOG_BTN_W - 12;
    } else {
        ok_x     = dx + (DIALOG_W - DIALOG_BTN_W) / 2;
        cancel_x = -1000;
    }

    g_dlg.hover_button = 0;
    if (my >= btn_y && my < btn_y + DIALOG_BTN_H) {
        if (mx >= ok_x && mx < ok_x + DIALOG_BTN_W) g_dlg.hover_button = 1;
        else if (g_dlg.has_cancel && mx >= cancel_x && mx < cancel_x + DIALOG_BTN_W) g_dlg.hover_button = 2;
    }

    if (clicked) {
        if (g_dlg.hover_button == 1)      finish(GUI_DIALOG_RESULT_OK);
        else if (g_dlg.hover_button == 2) finish(GUI_DIALOG_RESULT_CANCEL);
    }

    // Swallow every click while modal, including ones outside the frame.
    return true;
}

bool gui_dialog_handle_key(uint16_t key) {
    if (!g_dlg.open) return false;

    char c = (char)(key & 0xFF);
    if (c == '\n' || c == '\r') {
        finish(GUI_DIALOG_RESULT_OK);
    } else if (c == 27) { // Escape
        finish(g_dlg.has_cancel ? GUI_DIALOG_RESULT_CANCEL : GUI_DIALOG_RESULT_OK);
    }
    return true;
}

static void draw_button(int x, int y, const char* label, bool hovered, uint32_t accent) {
    gui_gfx_fill_rect(x, y, DIALOG_BTN_W, DIALOG_BTN_H,
                      hovered ? GUI_THEME_BG_ELEVATED : GUI_THEME_BG_SURFACE);
    gui_gfx_draw_rect(x, y, DIALOG_BTN_W, DIALOG_BTN_H, hovered ? accent : GUI_THEME_BORDER);

    int tw = (int)strlen(label) * 8;
    gui_gfx_draw_string(x + (DIALOG_BTN_W - tw) / 2, y + (DIALOG_BTN_H - 8) / 2,
                        label, hovered ? GUI_COLOR_WHITE : GUI_THEME_TEXT_MUTED);
}

void gui_dialog_render(void) {
    if (!g_dlg.open) return;

    // Dim the whole desktop so the modal reads as the only live surface.
    gui_gfx_fill_rect_blend(0, 0, gui_gfx_get_width(), gui_gfx_get_height(),
                            GUI_COLOR_BLACK, 120);

    int dx, dy;
    dialog_geometry(&dx, &dy);

    uint32_t accent;
    gui_icon_id_t icon;
    switch (g_dlg.kind) {
        case GUI_DIALOG_WARNING: accent = GUI_THEME_WARNING; icon = GUI_ICON_WARNING; break;
        case GUI_DIALOG_ERROR:   accent = GUI_THEME_DANGER;  icon = GUI_ICON_WARNING; break;
        case GUI_DIALOG_CONFIRM: accent = GUI_THEME_ACCENT;  icon = GUI_ICON_INFO;    break;
        default:                 accent = GUI_THEME_PRIMARY; icon = GUI_ICON_INFO;    break;
    }

    gui_gfx_draw_shadow(dx, dy, DIALOG_W, DIALOG_H, 10);
    gui_gfx_fill_rect(dx, dy, DIALOG_W, DIALOG_H, GUI_THEME_BG_SURFACE);
    gui_gfx_draw_rect(dx, dy, DIALOG_W, DIALOG_H, accent);

    // Title bar
    gui_gfx_draw_gradient_v(dx + 1, dy + 1, DIALOG_W - 2, DIALOG_TITLEBAR - 1,
                            GUI_THEME_BG_ELEVATED, GUI_THEME_BG_SURFACE);
    gui_gfx_draw_line(dx, dy + DIALOG_TITLEBAR, dx + DIALOG_W - 1, dy + DIALOG_TITLEBAR, accent);
    gui_gfx_draw_string_16_shadow(dx + 10, dy + 4, g_dlg.title, GUI_COLOR_WHITE, GUI_COLOR_BLACK);

    // Icon + wrapped message
    gui_icon_draw_scaled(icon, dx + 14, dy + DIALOG_TITLEBAR + 14, 2);
    for (int i = 0; i < g_dlg.line_count; i++) {
        gui_gfx_draw_string(dx + 58, dy + DIALOG_TITLEBAR + 14 + i * 13,
                            g_dlg.lines[i], GUI_THEME_TEXT_MAIN);
    }

    int btn_y = dy + DIALOG_H - DIALOG_BTN_H - 12;
    if (g_dlg.has_cancel) {
        draw_button(dx + DIALOG_W - (DIALOG_BTN_W * 2) - 22, btn_y, "OK",
                    g_dlg.hover_button == 1, accent);
        draw_button(dx + DIALOG_W - DIALOG_BTN_W - 12, btn_y, "Cancel",
                    g_dlg.hover_button == 2, GUI_THEME_DANGER);
    } else {
        draw_button(dx + (DIALOG_W - DIALOG_BTN_W) / 2, btn_y, "OK",
                    g_dlg.hover_button == 1, accent);
    }

    gui_gfx_draw_string(dx + 12, dy + DIALOG_H - 12, "Enter = OK   Esc = Dismiss",
                        GUI_THEME_TEXT_DIM);
}
