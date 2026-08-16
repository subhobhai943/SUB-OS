#include <drivers/vga.h>
#include <arch/x86_64/io.h>
#include <lib/string.h>

static volatile uint16_t* const VGA_BUFFER = (uint16_t*)VGA_MEMORY;
static uint8_t vga_color = 0x07;
static int vga_row = 0;
static int vga_col = 0;

void vga_set_cursor(int row, int col) {
    vga_row = row;
    vga_col = col;
    uint16_t pos = row * VGA_WIDTH + col;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_get_cursor(int* row, int* col) {
    if (row) *row = vga_row;
    if (col) *col = vga_col;
}

void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void vga_disable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    vga_color = vga_entry_color(fg, bg);
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', vga_color);
        }
    }
    vga_set_cursor(0, 0);
}

static void vga_scroll(void) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
    }
    vga_row = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row >= VGA_HEIGHT) {
            vga_scroll();
        }
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            if (++vga_row >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
        }
    } else {
        VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_color);
        if (++vga_col >= VGA_WIDTH) {
            vga_col = 0;
            if (++vga_row >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
    }
    vga_set_cursor(vga_row, vga_col);
}

void vga_write(const char* str) {
    while (*str) {
        vga_putc(*str++);
    }
}

void vga_init(void) {
    vga_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
    vga_enable_cursor(14, 15);
}
