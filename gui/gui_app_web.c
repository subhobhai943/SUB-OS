/*
 * SUB-OS Web -- a minimal text web browser.
 *
 * It is the visible end of the kernel's TCP client: type a URL, and the page
 * is fetched over the stack SUB-OS built. The fetch itself is blocking, so it
 * runs on the kernel's async HTTP worker thread (net/http_client) rather than
 * in the compositor -- the desktop keeps painting at full frame rate while a
 * page loads. This window only ever polls a job's state and renders the
 * result, so it never stalls.
 *
 * Two views of the response: Raw shows the status line, headers and body
 * exactly as received; Reader strips the markup and reflows the body into
 * wrapped text. The rendered line index is rebuilt only when the content, the
 * view or the window width changes, so scrolling and idle repaints are cheap.
 */

#include <gui/gui_apps.h>
#include <gui/gui_widgets.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <kernel/ktime.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <net/http_client.h>
#include <net/net.h>
#include <drivers/keyboard.h>

#define WEB_FETCH_CAP  16384   // response bytes retained
#define WEB_MAX_LINES  2048    // rendered display lines retained
#define WEB_URL_FIELD  1

typedef struct {
    char          url[256];
    http_fetch_t* job;
    bool          reader;      // strip markup and reflow
    int           scroll;      // first visible display line

    // Rendered line index, cached against the inputs that determine it.
    char*         text;                 // processed text being displayed
    int           text_len;
    int           offsets[WEB_MAX_LINES];
    int           lengths[WEB_MAX_LINES];
    int           nlines;

    const char*   cache_src;    // job->buf the cache was built from
    int           cache_len;
    bool          cache_reader;
    int           cache_cpl;    // columns the cache was wrapped to
} web_data_t;

// ===========================================================================
// HTML to text
// ===========================================================================
static bool tag_is_break(const char* tag) {
    // Block-level tags whose close (or self) should start a new line.
    static const char* const brk[] = {
        "br", "/p", "p", "/div", "/h1", "/h2", "/h3", "/h4", "/h5", "/h6",
        "/li", "/tr", "/ul", "/ol", "/table", "hr", "/title", "/head"
    };
    for (size_t i = 0; i < sizeof(brk) / sizeof(brk[0]); i++) {
        const char* b = brk[i];
        size_t n = strlen(b);
        // Match the tag name up to a space, '>' or end, case-insensitively.
        size_t k = 0;
        while (k < n && tag[k]) {
            char a = tag[k], c = b[k];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (a != c) break;
            k++;
        }
        if (k == n) {
            char after = tag[k];
            if (after == '\0' || after == ' ' || after == '>' || after == '/') return true;
        }
    }
    return false;
}

// Decode the handful of entities worth handling; returns the byte, or 0 if the
// entity is unknown (in which case the caller keeps it verbatim).
static char decode_entity(const char* e, int len) {
    if (len == 2 && strncmp(e, "lt", 2) == 0) return '<';
    if (len == 2 && strncmp(e, "gt", 2) == 0) return '>';
    if (len == 3 && strncmp(e, "amp", 3) == 0) return '&';
    if (len == 4 && strncmp(e, "nbsp", 4) == 0) return ' ';
    if (len == 4 && strncmp(e, "quot", 4) == 0) return '"';
    if (len == 4 && strncmp(e, "apos", 4) == 0) return '\'';
    if (len == 5 && strncmp(e, "mdash", 5) == 0) return '-';
    if (len == 5 && strncmp(e, "ndash", 5) == 0) return '-';
    return 0;
}

// Strip markup from `src` into `out` (bounded by cap), collapsing runs of
// whitespace and turning block tags into single newlines. <script>/<style>
// contents are dropped wholesale. Returns the output length.
static int html_to_text(const char* src, int slen, char* out, int cap) {
    int o = 0;
    bool sp_pending = false;   // a space owed but not yet emitted
    bool at_line_start = true;

    for (int i = 0; i < slen && o < cap - 1; i++) {
        char ch = src[i];

        if (ch == '<') {
            const char* tag = src + i + 1;

            // Skip <script>...</script> and <style>...</style> bodies entirely.
            if (strncmp(tag, "script", 6) == 0 || strncmp(tag, "style", 5) == 0) {
                const char* close = (tag[0] == 's' && tag[1] == 'c') ? "</script" : "</style";
                int cl = (int)strlen(close);
                int j = i + 1;
                while (j < slen && strncmp(src + j, close, cl) != 0) j++;
                i = j;
                while (i < slen && src[i] != '>') i++;
                continue;
            }

            bool brk = tag_is_break(tag);
            while (i < slen && src[i] != '>') i++;   // consume to tag close

            if (brk && !at_line_start) {
                out[o++] = '\n';
                at_line_start = true;
                sp_pending = false;
            }
            continue;
        }

        if (ch == '&') {
            int j = i + 1;
            while (j < slen && src[j] != ';' && j - i < 8) j++;
            char d = (j < slen && src[j] == ';') ? decode_entity(src + i + 1, j - i - 1) : 0;
            if (d) {
                if (sp_pending && !at_line_start) { out[o++] = ' '; sp_pending = false; }
                if (o < cap - 1) out[o++] = d;
                at_line_start = false;
                i = j;
                continue;
            }
            // Unknown entity: fall through and treat '&' as an ordinary char.
        }

        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            if (!at_line_start) sp_pending = true;
            continue;
        }

        if (sp_pending) { out[o++] = ' '; sp_pending = false; if (o >= cap - 1) break; }
        out[o++] = ch;
        at_line_start = false;
    }

    out[o] = '\0';
    return o;
}

// ===========================================================================
// Line breaking
// ===========================================================================
static void web_build_lines(web_data_t* wd, int cpl) {
    wd->nlines = 0;
    if (cpl < 8) cpl = 8;

    int n = wd->text_len;
    const char* t = wd->text;
    int i = 0;

    while (i < n && wd->nlines < WEB_MAX_LINES) {
        int start = i;
        int last_space = -1;
        int col = 0;

        while (i < n && t[i] != '\n' && col < cpl) {
            if (t[i] == ' ') last_space = i;
            i++;
            col++;
        }

        int end;
        if (i < n && t[i] == '\n') {
            end = i;
            i++;                             // consume the newline
        } else if (col >= cpl && i < n) {
            if (last_space > start) {         // wrap at the last space
                end = last_space;
                i = last_space + 1;
            } else {
                end = i;                      // a word longer than a line
            }
        } else {
            end = i;                          // reached the end of the text
        }

        wd->offsets[wd->nlines] = start;
        wd->lengths[wd->nlines] = end - start;
        wd->nlines++;
    }
}

// Rebuild the processed text and line index if any input changed.
static void web_refresh_cache(web_data_t* wd, int cpl) {
    http_fetch_t* j = wd->job;
    if (!j || j->state != HTTP_STATE_DONE) { wd->nlines = 0; return; }

    if (wd->cache_src == j->buf && wd->cache_len == j->len &&
        wd->cache_reader == wd->reader && wd->cache_cpl == cpl) {
        return;   // still valid
    }

    if (wd->reader) {
        // Reader view: skip the headers, then strip markup from the body.
        const char* body = j->buf;
        char* sep = strstr(j->buf, "\r\n\r\n");
        if (sep) body = sep + 4;
        int blen = j->len - (int)(body - j->buf);
        if (blen < 0) blen = 0;
        wd->text_len = html_to_text(body, blen, wd->text, WEB_FETCH_CAP);
    } else {
        // Raw view: the response verbatim, with tabs normalised.
        int n = j->len < WEB_FETCH_CAP - 1 ? j->len : WEB_FETCH_CAP - 1;
        for (int k = 0; k < n; k++) {
            char c = j->buf[k];
            wd->text[k] = (c == '\t') ? ' ' : c;
        }
        wd->text[n] = '\0';
        wd->text_len = n;
    }

    web_build_lines(wd, cpl);

    wd->cache_src    = j->buf;
    wd->cache_len    = j->len;
    wd->cache_reader = wd->reader;
    wd->cache_cpl    = cpl;
    if (wd->scroll > wd->nlines - 1) wd->scroll = 0;
}

// ===========================================================================
// Fetching
// ===========================================================================
static void web_go(web_data_t* wd) {
    if (wd->url[0] == '\0') return;

    // Drop any previous job (releasing a still-running one hands it to the
    // worker to free) and invalidate the render cache.
    if (wd->job) { http_fetch_release(wd->job); wd->job = NULL; }
    wd->cache_src = NULL;
    wd->scroll    = 0;

    wd->job = http_fetch_start(wd->url, WEB_FETCH_CAP);
    // A NULL result (worker busy or bad URL) is reflected in the status line
    // drawn from the live job state, so nothing more to do here.
}

// ===========================================================================
// Paint
// ===========================================================================
static void web_paint(gui_window_t* win) {
    web_data_t* wd = (web_data_t*)win->user_data;
    if (!wd) return;

    gui_widget_begin(win);
    int w  = gui_widget_client_width();
    int h  = gui_widget_client_height();
    int ox = win->x + 1;
    int oy = win->y + GUI_TITLEBAR_HEIGHT + 1;
    if (w < 240 || h < 120) { gui_widget_end(); return; }

    // --- toolbar ----------------------------------------------------------
    int go_w = 40, mode_w = 68;
    int field_w = w - 20 - go_w - mode_w - 12;
    if (gui_textfield(WEB_URL_FIELD, 10, 8, field_w, wd->url, sizeof(wd->url))) {
        /* editing; nothing to do until Go */
    }

    // Enter in the focused URL field triggers the fetch: the field leaves
    // newline keys unconsumed, so they are still visible here.
    const gui_widget_input_t* in = gui_widget_get_input();
    if (in->focus_id == WEB_URL_FIELD && (in->key == '\n' || in->key == '\r')) {
        gui_widget_set_focus(0);
        web_go(wd);
    }

    if (gui_button(2, 10 + field_w + 4, 8, go_w, 20, "Go")) web_go(wd);

    if (gui_button_colored(3, 10 + field_w + 4 + go_w + 4, 8, mode_w, 20,
                           wd->reader ? "Reader" : "Raw",
                           wd->reader ? GUI_THEME_SUCCESS : GUI_THEME_ACCENT)) {
        wd->reader = !wd->reader;
        wd->cache_src = NULL;   // force a re-render in the new view
        wd->scroll = 0;
    }

    // --- status line ------------------------------------------------------
    int bar_y = 34;
    char status[128];
    uint32_t status_col = GUI_THEME_TEXT_MUTED;

    if (!wd->job) {
        snprintf(status, sizeof(status), "Enter a URL and press Go  (e.g. 10.0.2.2:8000/)");
    } else if (wd->job->state == HTTP_STATE_RUNNING) {
        // A small spinner off the 10 Hz repaint so a slow fetch looks alive.
        static const char frames[] = { '|', '/', '-', '\\' };
        char sp = frames[(ktime_ms() / 120) % 4];
        snprintf(status, sizeof(status), "%c  fetching %s ...", sp, wd->job->host);
        status_col = GUI_THEME_WARNING;
    } else if (wd->job->state == HTTP_STATE_ERROR) {
        snprintf(status, sizeof(status), "error: %s", wd->job->err);
        status_col = GUI_THEME_DANGER;
    } else {
        char ips[16];
        ip_to_str(wd->job->ip, ips);
        snprintf(status, sizeof(status), "HTTP %d   %d bytes   %u ms   %s:%u (%s)",
                 wd->job->status_code, wd->job->len, wd->job->elapsed_ms,
                 wd->job->host, wd->job->port, ips);
        status_col = (wd->job->status_code == 200) ? GUI_THEME_SUCCESS : GUI_THEME_TEXT_MAIN;
    }
    gui_label(10, bar_y, status, status_col);
    gui_separator(10, bar_y + 14, w - 20);

    // --- page body --------------------------------------------------------
    int view_x = 10;
    int view_y = bar_y + 20;
    int view_h = h - view_y - 6;
    int line_h = 10;
    int cpl    = (w - 20 - 12) / GUI_FONT_W;   // leave room for the scrollbar
    if (cpl < 8) cpl = 8;
    int rows   = view_h / line_h;
    if (rows < 1) rows = 1;

    gui_gfx_fill_rect(ox + view_x, oy + view_y, w - 20, view_h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(ox + view_x, oy + view_y, w - 20, view_h, GUI_THEME_BORDER);

    web_refresh_cache(wd, cpl);

    if (wd->job && wd->job->state == HTTP_STATE_DONE && wd->nlines > 0) {
        if (wd->scroll > wd->nlines - 1) wd->scroll = wd->nlines - 1;
        if (wd->scroll < 0) wd->scroll = 0;

        char line[200];
        for (int r = 0; r < rows; r++) {
            int idx = wd->scroll + r;
            if (idx >= wd->nlines) break;

            int len = wd->lengths[idx];
            if (len > (int)sizeof(line) - 1) len = (int)sizeof(line) - 1;
            memcpy(line, wd->text + wd->offsets[idx], len);
            line[len] = '\0';

            // In raw view the header block (up to the blank line) is dimmed so
            // the body stands out; reader view is uniform.
            uint32_t col = GUI_THEME_TEXT_MAIN;
            if (!wd->reader && idx < wd->nlines) {
                if (len == 0) col = GUI_THEME_TEXT_DIM;
            }
            gui_gfx_draw_string(ox + view_x + 6, oy + view_y + 5 + r * line_h,
                                line, col);
        }

        // Scrollbar and keyboard paging.
        gui_scrollbar(4, w - 18, view_y, view_h, &wd->scroll, wd->nlines, rows);

        if (in->focus_id != WEB_URL_FIELD) {
            if (in->key == KEY_DOWN)      wd->scroll += 1;
            else if (in->key == KEY_UP)   wd->scroll -= 1;
            else if (in->key == KEY_PAGE_DOWN) wd->scroll += rows;
            else if (in->key == KEY_PAGE_UP)   wd->scroll -= rows;
        }
    } else if (wd->job && wd->job->state == HTTP_STATE_RUNNING) {
        gui_gfx_draw_string(ox + view_x + 8, oy + view_y + 10,
                            "Loading over TCP -- the desktop stays responsive.",
                            GUI_THEME_TEXT_DIM);
    }

    gui_widget_end();
}

static void web_event(gui_window_t* win, const gui_event_t* ev) {
    gui_widget_feed_event(win, ev);

    if (ev->type == GUI_EVENT_CLOSE && win->user_data) {
        web_data_t* wd = (web_data_t*)win->user_data;
        if (wd->job) http_fetch_release(wd->job);   // safe mid-fetch
        if (wd->text) kfree(wd->text);
        kfree(wd);
        win->user_data = NULL;
    }
}

void gui_app_web_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Web", x, y,
                                             (w > 0) ? w : 600, (h > 0) ? h : 420);
    if (!win) return;

    web_data_t* wd = (web_data_t*)kzalloc(sizeof(web_data_t));
    if (!wd) { gui_wm_destroy_window(win->id); return; }

    wd->text = (char*)kzalloc(WEB_FETCH_CAP);
    if (!wd->text) { kfree(wd); gui_wm_destroy_window(win->id); return; }

    wd->reader = true;
    strcpy(wd->url, "10.0.2.2:8000/");   // the host, reachable under QEMU slirp

    win->user_data    = wd;
    win->paint        = web_paint;
    win->handle_event = web_event;
}
