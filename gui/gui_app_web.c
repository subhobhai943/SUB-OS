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
#include <lib/image.h>
#include <drivers/keyboard.h>

#define WEB_FETCH_CAP  131072  // response bytes retained (fits images and long pages)
#define WEB_MAX_LINES  2048    // rendered display lines retained
#define WEB_MAX_LINKS  400     // hyperlinks tracked per page
#define WEB_HREF_POOL  6144    // bytes of href strings per page
#define WEB_HISTORY    16      // back-stack depth
#define WEB_URL_FIELD  1

// A hyperlink: the [start,end) span it occupies in the processed text, and the
// offset of its href string in the pool.
typedef struct {
    int start;
    int end;
    int href;
} web_link_t;

typedef struct {
    char          url[256];
    http_fetch_t* job;
    bool          reader;      // strip markup and reflow
    int           scroll;      // first visible display line

    // Rendered text and its line index, cached against the inputs that
    // determine them.
    char*         text;                 // processed text being displayed
    int           text_len;
    int           offsets[WEB_MAX_LINES];
    int           lengths[WEB_MAX_LINES];
    int           nlines;

    // Links found while stripping markup (reader view only).
    web_link_t    links[WEB_MAX_LINKS];
    int           nlinks;
    char          hrefs[WEB_HREF_POOL];
    int           href_used;

    // The page's own address, used to resolve relative links.
    char          base_host[128];
    uint16_t      base_port;
    char          base_path[256];

    // Back stack of previously-visited URLs.
    char          hist[WEB_HISTORY][256];
    int           hist_len;

    // When the response is an image, it is decoded once into here and shown
    // instead of text.
    bool          has_image;
    image_t       image;

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

// Copy the href="..." value out of an anchor tag's body (the text between the
// '<' and its '>') into the page's href pool, and return its pool offset, or -1
// if there is no usable href or the pool is full.
static int capture_href(web_data_t* wd, const char* tag, int taglen) {
    // Find "href" (case-insensitive) followed by '='.
    int h = -1;
    for (int i = 0; i + 4 <= taglen; i++) {
        char a = tag[i],   b = tag[i + 1], c = tag[i + 2], d = tag[i + 3];
        if ((a | 32) == 'h' && (b | 32) == 'r' && (c | 32) == 'e' && (d | 32) == 'f') {
            int j = i + 4;
            while (j < taglen && (tag[j] == ' ' || tag[j] == '\t')) j++;
            if (j < taglen && tag[j] == '=') { h = j + 1; break; }
        }
    }
    if (h < 0) return -1;

    while (h < taglen && (tag[h] == ' ' || tag[h] == '\t')) h++;
    char quote = 0;
    if (h < taglen && (tag[h] == '"' || tag[h] == '\'')) { quote = tag[h]; h++; }

    if (wd->href_used >= WEB_HREF_POOL - 1) return -1;
    int off = wd->href_used;
    int o = off;
    while (h < taglen && o < WEB_HREF_POOL - 1) {
        char c = tag[h];
        if (quote ? (c == quote) : (c == ' ' || c == '\t' || c == '>')) break;
        wd->hrefs[o++] = c;
        h++;
    }
    wd->hrefs[o++] = '\0';
    if (o == off + 1) return -1;   // empty href
    wd->href_used = o;
    return off;
}

// Strip markup from `src` into wd->text, collapsing whitespace and turning
// block tags into newlines. <script>/<style> bodies are dropped. Anchors are
// recorded as links spanning the text they enclose. Returns the text length.
static int html_to_text(web_data_t* wd, const char* src, int slen) {
    char* out = wd->text;
    int cap = WEB_FETCH_CAP;
    int o = 0;
    bool sp_pending = false;   // a space owed but not yet emitted
    bool at_line_start = true;

    bool link_open = false;    // inside an <a>...</a>
    int  link_start = 0;
    int  link_href = -1;

    wd->nlinks = 0;
    wd->href_used = 0;

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
            bool a_open  = (tag[0] | 32) == 'a' &&
                           (tag[1] == ' ' || tag[1] == '\t' || tag[1] == '>');
            bool a_close = tag[0] == '/' && (tag[1] | 32) == 'a' &&
                           (tag[2] == '>' || tag[2] == ' ');

            int tstart = i + 1;
            while (i < slen && src[i] != '>') i++;   // consume to tag close
            int tlen = i - tstart;

            if (a_open && !link_open) {
                link_href  = capture_href(wd, src + tstart, tlen);
                link_open  = (link_href >= 0);
                link_start = o;
            } else if (a_close && link_open) {
                if (o > link_start && wd->nlinks < WEB_MAX_LINKS) {
                    wd->links[wd->nlinks].start = link_start;
                    wd->links[wd->nlinks].end   = o;
                    wd->links[wd->nlinks].href  = link_href;
                    wd->nlinks++;
                }
                link_open = false;
            }

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

    // Remember the page's own address for resolving relative links.
    strncpy(wd->base_host, j->host, sizeof(wd->base_host) - 1);
    wd->base_host[sizeof(wd->base_host) - 1] = '\0';
    strncpy(wd->base_path, j->path, sizeof(wd->base_path) - 1);
    wd->base_path[sizeof(wd->base_path) - 1] = '\0';
    wd->base_port = j->port;

    // If the body is an image, decode it once and show it instead of text.
    if (wd->has_image) image_free(&wd->image);
    wd->has_image = false;
    {
        const uint8_t* body = (const uint8_t*)j->buf;
        char* sep = strstr(j->buf, "\r\n\r\n");
        if (sep) body = (const uint8_t*)(sep + 4);
        int blen = j->len - (int)((const char*)body - j->buf);
        if (blen > 0 && image_sniff(body, blen)) {
            if (image_decode(body, blen, &wd->image) == 0) wd->has_image = true;
        }
    }
    if (wd->has_image) {
        wd->nlines = 0;
        wd->cache_src = j->buf; wd->cache_len = j->len;
        wd->cache_reader = wd->reader; wd->cache_cpl = cpl;
        return;                                   // image branch: no text layout
    }

    if (wd->reader) {
        // Reader view: skip the headers, then strip markup from the body,
        // recording anchors as links as it goes.
        const char* body = j->buf;
        char* sep = strstr(j->buf, "\r\n\r\n");
        if (sep) body = sep + 4;
        int blen = j->len - (int)(body - j->buf);
        if (blen < 0) blen = 0;
        wd->text_len = html_to_text(wd, body, blen);
    } else {
        // Raw view: the response verbatim, with tabs normalised. No links.
        int n = j->len < WEB_FETCH_CAP - 1 ? j->len : WEB_FETCH_CAP - 1;
        for (int k = 0; k < n; k++) {
            char c = j->buf[k];
            wd->text[k] = (c == '\t') ? ' ' : c;
        }
        wd->text[n] = '\0';
        wd->text_len = n;
        wd->nlinks = 0;
    }

    web_build_lines(wd, cpl);

    wd->cache_src    = j->buf;
    wd->cache_len    = j->len;
    wd->cache_reader = wd->reader;
    wd->cache_cpl    = cpl;
    if (wd->scroll > wd->nlines - 1) wd->scroll = 0;
}

// ===========================================================================
// Fetching and navigation
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

// Resolve a link's href against the current page into an absolute URL. Returns
// false for links that are not navigable here (fragments, mailto:, javascript:,
// empty). https:// is resolved through and left for the fetch layer to refuse
// with a clear message rather than silently dropped.
static bool web_resolve(web_data_t* wd, const char* href, char* out, size_t cap) {
    if (!href || href[0] == '\0' || href[0] == '#') return false;
    if (strncmp(href, "mailto:", 7) == 0 || strncmp(href, "javascript:", 11) == 0) {
        return false;
    }

    if (strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0) {
        strncpy(out, href, cap - 1);
        out[cap - 1] = '\0';
        return true;
    }

    // Port suffix only when it is not the default 80.
    char portbuf[8] = "";
    if (wd->base_port != 80) snprintf(portbuf, sizeof(portbuf), ":%u", wd->base_port);

    if (href[0] == '/') {
        // Root-relative: keep host, replace path.
        snprintf(out, cap, "http://%s%s%s", wd->base_host, portbuf, href);
        return true;
    }

    // Document-relative: append to the current path's directory.
    char dir[256];
    strncpy(dir, wd->base_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char* slash = strrchr(dir, '/');
    if (slash) slash[1] = '\0';   // keep the trailing slash
    else       strcpy(dir, "/");
    snprintf(out, cap, "http://%s%s%s%s", wd->base_host, portbuf, dir, href);
    return true;
}

// Follow a link: remember where we are, then load the target.
static void web_navigate(web_data_t* wd, const char* href) {
    char target[256];
    if (!web_resolve(wd, href, target, sizeof(target))) return;

    // Push the current address onto the back stack (dropping the oldest if it
    // is full), unless we have not loaded anything yet.
    if (wd->url[0]) {
        if (wd->hist_len == WEB_HISTORY) {
            for (int i = 1; i < WEB_HISTORY; i++) {
                strcpy(wd->hist[i - 1], wd->hist[i]);
            }
            wd->hist_len--;
        }
        strncpy(wd->hist[wd->hist_len], wd->url, sizeof(wd->hist[0]) - 1);
        wd->hist[wd->hist_len][sizeof(wd->hist[0]) - 1] = '\0';
        wd->hist_len++;
    }

    strncpy(wd->url, target, sizeof(wd->url) - 1);
    wd->url[sizeof(wd->url) - 1] = '\0';
    web_go(wd);
}

static void web_back(web_data_t* wd) {
    if (wd->hist_len == 0) return;
    wd->hist_len--;
    strncpy(wd->url, wd->hist[wd->hist_len], sizeof(wd->url) - 1);
    wd->url[sizeof(wd->url) - 1] = '\0';
    web_go(wd);
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
    int back_w = 26, go_w = 34, mode_w = 62;
    int field_x = 10 + back_w + 4;
    int field_w = w - field_x - 4 - go_w - 4 - mode_w - 8;
    if (field_w < 60) field_w = 60;

    // Back is enabled only when there is somewhere to go back to.
    bool can_back = wd->hist_len > 0;
    if (gui_button_colored(6, 10, 8, back_w, 20, "<",
                           can_back ? GUI_THEME_PRIMARY : GUI_THEME_BORDER) && can_back) {
        web_back(wd);
    }

    if (gui_textfield(WEB_URL_FIELD, field_x, 8, field_w, wd->url, sizeof(wd->url))) {
        /* editing; nothing to do until Go */
    }

    // Enter in the focused URL field triggers the fetch: the field leaves
    // newline keys unconsumed, so they are still visible here.
    const gui_widget_input_t* in = gui_widget_get_input();
    if (in->focus_id == WEB_URL_FIELD && (in->key == '\n' || in->key == '\r')) {
        gui_widget_set_focus(0);
        web_go(wd);
    }

    if (gui_button(2, field_x + field_w + 4, 8, go_w, 20, "Go")) web_go(wd);

    if (gui_button_colored(3, field_x + field_w + 4 + go_w + 4, 8, mode_w, 20,
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

    if (wd->has_image && wd->job && wd->job->state == HTTP_STATE_DONE) {
        image_t* im = &wd->image;

        int avail_w = (w - 20) - 12;                 // panel minus scrollbar
        if (avail_w < 16) avail_w = 16;
        int sw = im->width, sh = im->height;
        if (sw > avail_w) { sh = (int)((long)sh * avail_w / sw); sw = avail_w; }
        if (sh < 1) sh = 1;

        int page = view_h - 6;
        if (wd->scroll > sh - 1) wd->scroll = sh - 1;
        if (wd->scroll < 0) wd->scroll = 0;

        int draw_x  = ox + view_x + 6;
        int draw_y0 = oy + view_y + 4;

        // Column map (screen column -> source column) so the inner loop has no
        // divide. Bounded by the panel width.
        static int xmap[1920];
        int cols = sw < (int)(sizeof(xmap) / sizeof(xmap[0])) ? sw
                                                              : (int)(sizeof(xmap) / sizeof(xmap[0]));
        for (int sx = 0; sx < cols; sx++) xmap[sx] = (int)((long)sx * im->width / sw);

        for (int r = 0; r < page; r++) {
            int scaled_y = wd->scroll + r;
            if (scaled_y >= sh) break;
            int iy = (int)((long)scaled_y * im->height / sh);
            if (iy >= im->height) break;
            const uint32_t* srow = im->pixels + (long)iy * im->width;
            int py = draw_y0 + r;
            for (int sx = 0; sx < cols; sx++) {
                uint32_t px = srow[xmap[sx]];
                uint8_t a = (uint8_t)(px >> 24);
                uint32_t rgb = 0xFF000000u | (px & 0x00FFFFFFu);
                uint32_t col = (a == 255) ? rgb
                             : gui_color_alpha_blend(GUI_THEME_BG_DARK, rgb, a);
                gui_gfx_draw_pixel(draw_x + sx, py, col);
            }
        }

        gui_scrollbar(4, w - 18, view_y, view_h, &wd->scroll, sh, page);

        if (in->focus_id != WEB_URL_FIELD) {
            if (in->key == KEY_DOWN)           wd->scroll += 16;
            else if (in->key == KEY_UP)        wd->scroll -= 16;
            else if (in->key == KEY_PAGE_DOWN) wd->scroll += page;
            else if (in->key == KEY_PAGE_UP)   wd->scroll -= page;
        }

        char hint[48];
        snprintf(hint, sizeof(hint), "image  %dx%d", im->width, im->height);
        gui_label_aligned(0, bar_y, w - 12, hint, GUI_THEME_ACCENT, GUI_ALIGN_RIGHT);
    } else if (wd->job && wd->job->state == HTTP_STATE_DONE && wd->nlines > 0) {
        if (wd->scroll > wd->nlines - 1) wd->scroll = wd->nlines - 1;
        if (wd->scroll < 0) wd->scroll = 0;

        // A click on a link's text follows it. Map the click to a text offset,
        // then to the link whose span contains it. Done before drawing so a
        // navigation takes hold immediately.
        if (wd->reader && wd->nlinks && in->mouse_clicked) {
            int rx = in->mouse_x - (view_x + 6);
            int ry = in->mouse_y - (view_y + 5);
            if (rx >= 0 && ry >= 0 && in->mouse_x < view_x + (w - 20) - 4 &&
                in->mouse_y < view_y + view_h) {
                int idx = wd->scroll + ry / line_h;
                int col = rx / GUI_FONT_W;
                if (idx >= 0 && idx < wd->nlines && col < wd->lengths[idx]) {
                    int off = wd->offsets[idx] + col;
                    for (int li = 0; li < wd->nlinks; li++) {
                        if (off >= wd->links[li].start && off < wd->links[li].end) {
                            web_navigate(wd, wd->hrefs + wd->links[li].href);
                            break;
                        }
                    }
                }
            }
        }

        char line[200];
        for (int r = 0; r < rows; r++) {
            int idx = wd->scroll + r;
            if (idx >= wd->nlines) break;

            int loff = wd->offsets[idx];
            int len  = wd->lengths[idx];
            if (len > (int)sizeof(line) - 1) len = (int)sizeof(line) - 1;
            memcpy(line, wd->text + loff, len);
            line[len] = '\0';

            int sx = ox + view_x + 6;
            int sy = oy + view_y + 5 + r * line_h;

            // In raw view the header block (up to the blank line) is dimmed so
            // the body stands out; reader view is uniform.
            uint32_t col = GUI_THEME_TEXT_MAIN;
            if (!wd->reader && len == 0) col = GUI_THEME_TEXT_DIM;
            gui_gfx_draw_string(sx, sy, line, col);

            // Overlay any link spans on this line in the accent colour, with an
            // underline, so hyperlinks read as clickable.
            if (wd->reader) {
                for (int li = 0; li < wd->nlinks; li++) {
                    int s = wd->links[li].start, e = wd->links[li].end;
                    if (e <= loff || s >= loff + len) continue;
                    int a = s > loff ? s : loff;
                    int b = e < loff + len ? e : loff + len;
                    int seglen = b - a;
                    if (seglen <= 0 || seglen > (int)sizeof(line) - 1) continue;

                    char seg[200];
                    memcpy(seg, wd->text + a, seglen);
                    seg[seglen] = '\0';
                    int segx = sx + (a - loff) * GUI_FONT_W;
                    gui_gfx_draw_string(segx, sy, seg, GUI_THEME_ACCENT);
                    gui_gfx_fill_rect(segx, sy + 9, seglen * GUI_FONT_W, 1, GUI_THEME_ACCENT);
                }
            }
        }

        // Scrollbar and keyboard paging.
        gui_scrollbar(4, w - 18, view_y, view_h, &wd->scroll, wd->nlines, rows);

        if (in->focus_id != WEB_URL_FIELD) {
            if (in->key == KEY_DOWN)      wd->scroll += 1;
            else if (in->key == KEY_UP)   wd->scroll -= 1;
            else if (in->key == KEY_PAGE_DOWN) wd->scroll += rows;
            else if (in->key == KEY_PAGE_UP)   wd->scroll -= rows;
        }

        // A link count hint, so it is clear the page is navigable.
        if (wd->reader && wd->nlinks) {
            char hint[48];
            snprintf(hint, sizeof(hint), "%d link%s - click to follow",
                     wd->nlinks, wd->nlinks == 1 ? "" : "s");
            gui_label_aligned(0, bar_y, w - 12, hint, GUI_THEME_ACCENT, GUI_ALIGN_RIGHT);
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
        if (wd->has_image) image_free(&wd->image);
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
    strcpy(wd->url, "https://example.com/"); // a real HTTPS page as the default

    win->user_data    = wd;
    win->paint        = web_paint;
    win->handle_event = web_event;
}
