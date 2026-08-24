// Framebuffer console for SUB-OS
//
// Draws the kernel console into the linear framebuffer with the shared 8x8
// font at double height. Carries enough of a VT100/ANSI parser -- SGR colours,
// cursor addressing, erase-display and erase-line -- that full-screen programs
// like the nano editor render correctly, which the VGA text console cannot do
// once the adapter has been switched into graphics mode.

#include <drivers/fbcon.h>
#include <drivers/fb.h>
#include <lib/font8x8.h>
#include <lib/string.h>

#define FBCON_MAX_PARAMS 6

// Standard ANSI palette, bright variants in the upper half.
static const uint32_t g_ansi_colors[16] = {
    0xFF1E293B, 0xFFEF4444, 0xFF22C55E, 0xFFEAB308,
    0xFF3B82F6, 0xFFA855F7, 0xFF06B6D4, 0xFFCBD5E1,
    0xFF64748B, 0xFFF87171, 0xFF4ADE80, 0xFFFBBF24,
    0xFF60A5FA, 0xFFC084FC, 0xFF22D3EE, 0xFFF8FAFC
};

#define FBCON_DEFAULT_FG 0xFFCBD5E1
#define FBCON_DEFAULT_BG 0xFF0B0F19

typedef enum {
    PARSE_NORMAL,
    PARSE_ESC,
    PARSE_CSI
} parse_state_t;

static bool     g_active = false;
static int      g_cols = 0, g_rows = 0;
static int      g_cell_w = 8, g_cell_h = 16;
static int      g_cur_x = 0, g_cur_y = 0;
static int      g_saved_x = 0, g_saved_y = 0;
static uint32_t g_fg = FBCON_DEFAULT_FG;
static uint32_t g_bg = FBCON_DEFAULT_BG;
static bool     g_bold = false;

static parse_state_t g_state = PARSE_NORMAL;
static int g_params[FBCON_MAX_PARAMS];
static int g_param_count = 0;
static bool g_param_seen = false;

// ---------------------------------------------------------------------------
// Glyph output
// ---------------------------------------------------------------------------

static void draw_cell(int col, int row, char c, uint32_t fg, uint32_t bg) {
    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->active) return;

    int x = col * g_cell_w;
    int y = row * g_cell_h;
    const uint8_t* glyph = font8x8_glyph(c);

    // Nearest-neighbour scale from the 8x8 face to whatever cell size the
    // current resolution divides into, so 80x25 holds at any mode.
    for (int dy = 0; dy < g_cell_h; dy++) {
        uint8_t bits = glyph[(dy * 8) / g_cell_h];
        for (int dx = 0; dx < g_cell_w; dx++) {
            int sx = (dx * 8) / g_cell_w;
            uint32_t color = (bits & (0x80u >> sx)) ? fg : bg;
            fb_put_pixel((uint32_t)(x + dx), (uint32_t)(y + dy), color);
        }
    }
}

static void clear_rows(int first_row, int count) {
    if (count <= 0) return;
    fb_draw_rect(0, (uint32_t)(first_row * g_cell_h),
                 (uint32_t)(g_cols * g_cell_w),
                 (uint32_t)(count * g_cell_h), g_bg);
}

// Scroll by moving framebuffer scanlines up one cell height; no shadow grid is
// kept, so the pixels themselves are the console's backing store.
static void scroll_one_line(void) {
    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->active || !fb->address) return;

    uint32_t width = fb->width;
    uint32_t shift = (uint32_t)g_cell_h;
    uint32_t used_height = (uint32_t)(g_rows * g_cell_h);

    for (uint32_t y = 0; y + shift < used_height; y++) {
        uint32_t* dst = fb->address + (size_t)y * width;
        uint32_t* src = fb->address + (size_t)(y + shift) * width;
        memcpy(dst, src, (size_t)g_cols * (size_t)g_cell_w * sizeof(uint32_t));
    }

    clear_rows(g_rows - 1, 1);
}

static void newline(void) {
    g_cur_x = 0;
    if (++g_cur_y >= g_rows) {
        scroll_one_line();
        g_cur_y = g_rows - 1;
    }
}

// ---------------------------------------------------------------------------
// ANSI handling
// ---------------------------------------------------------------------------

static void apply_sgr(void) {
    if (g_param_count == 0) {
        g_fg = FBCON_DEFAULT_FG;
        g_bg = FBCON_DEFAULT_BG;
        g_bold = false;
        return;
    }

    for (int i = 0; i < g_param_count; i++) {
        int code = g_params[i];

        if (code == 0) {
            g_fg = FBCON_DEFAULT_FG;
            g_bg = FBCON_DEFAULT_BG;
            g_bold = false;
        } else if (code == 1) {
            g_bold = true;
        } else if (code == 7) {
            uint32_t tmp = g_fg;   // Reverse video
            g_fg = g_bg;
            g_bg = tmp;
        } else if (code >= 30 && code <= 37) {
            g_fg = g_ansi_colors[(code - 30) + (g_bold ? 8 : 0)];
        } else if (code >= 90 && code <= 97) {
            g_fg = g_ansi_colors[(code - 90) + 8];
        } else if (code >= 40 && code <= 47) {
            g_bg = g_ansi_colors[code - 40];
        } else if (code >= 100 && code <= 107) {
            g_bg = g_ansi_colors[(code - 100) + 8];
        }
    }
}

static int param_or(int index, int fallback) {
    if (index >= g_param_count) return fallback;
    return g_params[index] ? g_params[index] : fallback;
}

static void handle_csi(char final) {
    switch (final) {
        case 'm':
            apply_sgr();
            break;

        case 'H': case 'f': {
            // Cursor position is 1-based on the wire, 0-based internally.
            int row = param_or(0, 1) - 1;
            int col = param_or(1, 1) - 1;
            g_cur_y = (row < 0) ? 0 : (row >= g_rows ? g_rows - 1 : row);
            g_cur_x = (col < 0) ? 0 : (col >= g_cols ? g_cols - 1 : col);
            break;
        }

        case 'A': g_cur_y -= param_or(0, 1); if (g_cur_y < 0) g_cur_y = 0; break;
        case 'B': g_cur_y += param_or(0, 1); if (g_cur_y >= g_rows) g_cur_y = g_rows - 1; break;
        case 'C': g_cur_x += param_or(0, 1); if (g_cur_x >= g_cols) g_cur_x = g_cols - 1; break;
        case 'D': g_cur_x -= param_or(0, 1); if (g_cur_x < 0) g_cur_x = 0; break;

        case 'J': {
            int mode = (g_param_count && g_param_seen) ? g_params[0] : 0;
            if (mode == 2 || mode == 3) {
                clear_rows(0, g_rows);
                g_cur_x = g_cur_y = 0;
            } else if (mode == 1) {
                clear_rows(0, g_cur_y);
            } else {
                clear_rows(g_cur_y + 1, g_rows - g_cur_y - 1);
            }
            break;
        }

        case 'K': {
            int mode = (g_param_count && g_param_seen) ? g_params[0] : 0;
            int from = (mode == 1) ? 0 : g_cur_x;
            int to   = (mode == 1) ? g_cur_x + 1 : g_cols;
            if (mode == 2) { from = 0; to = g_cols; }
            if (to > from) {
                fb_draw_rect((uint32_t)(from * g_cell_w),
                             (uint32_t)(g_cur_y * g_cell_h),
                             (uint32_t)((to - from) * g_cell_w),
                             (uint32_t)g_cell_h, g_bg);
            }
            break;
        }

        case 's': g_saved_x = g_cur_x; g_saved_y = g_cur_y; break;
        case 'u': g_cur_x = g_saved_x; g_cur_y = g_saved_y; break;

        default:
            break;   // Unsupported sequences are consumed and ignored
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void fbcon_init(void) {
    const fb_info_t* fb = fb_get_info();
    if (!fb) return;

    g_cols = FBCON_COLS;
    g_rows = FBCON_ROWS;
    g_cell_w = (int)(fb->width / (uint32_t)FBCON_COLS);
    g_cell_h = (int)(fb->height / (uint32_t)FBCON_ROWS);
    if (g_cell_w < 1) g_cell_w = 1;
    if (g_cell_h < 1) g_cell_h = 1;
    g_cur_x = g_cur_y = 0;
    g_fg = FBCON_DEFAULT_FG;
    g_bg = FBCON_DEFAULT_BG;
    g_bold = false;
    g_state = PARSE_NORMAL;
}

void fbcon_enable(bool enable) {
    if (enable) {
        fbcon_init();
        g_active = true;
        fbcon_clear();
    } else {
        g_active = false;
    }
}

bool fbcon_is_active(void) {
    return g_active;
}

int fbcon_cols(void) { return g_cols; }
int fbcon_rows(void) { return g_rows; }

void fbcon_set_cursor(int row, int col) {
    if (row >= 0 && row < g_rows) g_cur_y = row;
    if (col >= 0 && col < g_cols) g_cur_x = col;
}

void fbcon_get_cursor(int* row, int* col) {
    if (row) *row = g_cur_y;
    if (col) *col = g_cur_x;
}

void fbcon_clear(void) {
    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->active) return;

    fb_draw_rect(0, 0, fb->width, fb->height, g_bg);
    g_cur_x = g_cur_y = 0;
}

void fbcon_putc(char c) {
    if (!g_active || g_cols == 0 || g_rows == 0) return;

    switch (g_state) {
        case PARSE_ESC:
            if (c == '[') {
                g_state = PARSE_CSI;
                g_param_count = 0;
                g_param_seen = false;
                for (int i = 0; i < FBCON_MAX_PARAMS; i++) g_params[i] = 0;
            } else {
                g_state = PARSE_NORMAL;   // Two-character escape, ignored
            }
            return;

        case PARSE_CSI:
            if (c >= '0' && c <= '9') {
                if (g_param_count == 0) g_param_count = 1;
                g_params[g_param_count - 1] = g_params[g_param_count - 1] * 10 + (c - '0');
                g_param_seen = true;
            } else if (c == ';') {
                if (g_param_count < FBCON_MAX_PARAMS) g_param_count++;
            } else if (c == '?' || c == '>') {
                // Private-mode introducer; parameters still parse normally.
            } else {
                handle_csi(c);
                g_state = PARSE_NORMAL;
            }
            return;

        default:
            break;
    }

    switch (c) {
        case '\033': g_state = PARSE_ESC; return;
        case '\n':   newline(); return;
        case '\r':   g_cur_x = 0; return;
        case '\t':
            g_cur_x = (g_cur_x + 8) & ~7;
            if (g_cur_x >= g_cols) newline();
            return;
        case '\b':
            if (g_cur_x > 0) {
                g_cur_x--;
                draw_cell(g_cur_x, g_cur_y, ' ', g_fg, g_bg);
            }
            return;
        case '\a':
            return;
        default:
            break;
    }

    // Log-level markers (\001 followed by a digit) never reach the glass.
    if ((unsigned char)c < 32) return;

    draw_cell(g_cur_x, g_cur_y, c, g_fg, g_bg);
    if (++g_cur_x >= g_cols) newline();
}

void fbcon_write(const char* data, size_t len) {
    if (!data) return;
    for (size_t i = 0; i < len; i++) fbcon_putc(data[i]);
}
