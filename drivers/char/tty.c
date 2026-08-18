#include <drivers/tty.h>
#include <drivers/vga.h>
#include <lib/string.h>

typedef struct {
    uint16_t buffer[TTY_HEIGHT][TTY_WIDTH];
    int cursor_row;
    int cursor_col;
    uint8_t current_color;
} tty_t;

static tty_t ttys[MAX_TTYS];
static int current_tty = 0;

static void tty_render_current(void) {
#if defined(__x86_64__)
    volatile uint16_t* const vga_mem = (uint16_t*)VGA_MEMORY;
    tty_t* t = &ttys[current_tty];
    for (int y = 0; y < TTY_HEIGHT; y++) {
        for (int x = 0; x < TTY_WIDTH; x++) {
            vga_mem[y * TTY_WIDTH + x] = t->buffer[y][x];
        }
    }
    vga_set_cursor(t->cursor_row, t->cursor_col);
#endif
}

void tty_switch(int index) {
    if (index < 0 || index >= MAX_TTYS) return;
    current_tty = index;
    tty_render_current();
}

int tty_get_current(void) {
    return current_tty;
}

static void tty_scroll(tty_t* t) {
    for (int y = 0; y < TTY_HEIGHT - 1; y++) {
        for (int x = 0; x < TTY_WIDTH; x++) {
            t->buffer[y][x] = t->buffer[y + 1][x];
        }
    }
    for (int x = 0; x < TTY_WIDTH; x++) {
        t->buffer[TTY_HEIGHT - 1][x] = vga_entry(' ', t->current_color);
    }
    t->cursor_row = TTY_HEIGHT - 1;
}

void tty_clear(void) {
    tty_t* t = &ttys[current_tty];
    for (int y = 0; y < TTY_HEIGHT; y++) {
        for (int x = 0; x < TTY_WIDTH; x++) {
            t->buffer[y][x] = vga_entry(' ', t->current_color);
        }
    }
    t->cursor_row = 0;
    t->cursor_col = 0;
    tty_render_current();
}

void tty_set_cursor(int row, int col) {
    tty_t* t = &ttys[current_tty];
    t->cursor_row = row;
    t->cursor_col = col;
#if defined(__x86_64__)
    vga_set_cursor(row, col);
#endif
}

void tty_write(const char* data, size_t size) {
    tty_t* t = &ttys[current_tty];

    for (size_t i = 0; i < size; i++) {
        char c = data[i];

        // Basic ANSI escape sequence parser (e.g. \033[31m)
        if (c == '\033' && i + 1 < size && data[i + 1] == '[') {
            i += 2;
            int code = 0;
            while (i < size && data[i] >= '0' && data[i] <= '9') {
                code = code * 10 + (data[i] - '0');
                i++;
            }
            if (i < size && data[i] == 'm') {
                if (code == 0) t->current_color = 0x07; // Reset
                else if (code == 31) t->current_color = 0x04; // Red
                else if (code == 32) t->current_color = 0x02; // Green
                else if (code == 33) t->current_color = 0x06; // Brown/Yellow
                else if (code == 34) t->current_color = 0x01; // Blue
                else if (code == 35) t->current_color = 0x05; // Magenta
                else if (code == 36) t->current_color = 0x03; // Cyan
                else if (code == 37) t->current_color = 0x07; // White
                else if (code == 90) t->current_color = 0x08; // Bright Black (Dark Grey)
                else if (code == 91) t->current_color = 0x0C; // Bright Red
                else if (code == 92) t->current_color = 0x0A; // Bright Green
                else if (code == 93) t->current_color = 0x0E; // Bright Yellow
                else if (code == 94) t->current_color = 0x09; // Bright Blue
                else if (code == 95) t->current_color = 0x0D; // Bright Magenta
                else if (code == 96) t->current_color = 0x0B; // Bright Cyan
                else if (code == 97) t->current_color = 0x0F; // Bright White
            } else if (i < size && data[i] == 'K') {
                // Clear from cursor to end of line
                for (int x = t->cursor_col; x < TTY_WIDTH; x++) {
                    t->buffer[t->cursor_row][x] = vga_entry(' ', t->current_color);
                }
            } else if (i < size && data[i] == 'D') {
                // Move cursor left
                if (t->cursor_col > 0) t->cursor_col--;
            } else if (i < size && data[i] == 'C') {
                // Move cursor right
                if (t->cursor_col < TTY_WIDTH - 1) t->cursor_col++;
            }
            continue;
        }

        if (c == '\n') {
            t->cursor_col = 0;
            if (++t->cursor_row >= TTY_HEIGHT) {
                tty_scroll(t);
            }
        } else if (c == '\r') {
            t->cursor_col = 0;
        } else if (c == '\t') {
            t->cursor_col = (t->cursor_col + 8) & ~7;
            if (t->cursor_col >= TTY_WIDTH) {
                t->cursor_col = 0;
                if (++t->cursor_row >= TTY_HEIGHT) {
                    tty_scroll(t);
                }
            }
        } else if (c == '\b') {
            if (t->cursor_col > 0) {
                t->cursor_col--;
                t->buffer[t->cursor_row][t->cursor_col] = vga_entry(' ', t->current_color);
            }
        } else {
            t->buffer[t->cursor_row][t->cursor_col] = vga_entry(c, t->current_color);
            if (++t->cursor_col >= TTY_WIDTH) {
                t->cursor_col = 0;
                if (++t->cursor_row >= TTY_HEIGHT) {
                    tty_scroll(t);
                }
            }
        }
    }

    tty_render_current();
}

void tty_write_string(const char* data) {
    tty_write(data, strlen(data));
}

void tty_init(void) {
#if defined(__x86_64__)
    vga_init();
#endif
    for (int i = 0; i < MAX_TTYS; i++) {
        ttys[i].cursor_row = 0;
        ttys[i].cursor_col = 0;
        ttys[i].current_color = 0x07;
        for (int y = 0; y < TTY_HEIGHT; y++) {
            for (int x = 0; x < TTY_WIDTH; x++) {
                ttys[i].buffer[y][x] = vga_entry(' ', 0x07);
            }
        }
    }
    current_tty = 0;
    tty_render_current();
}
