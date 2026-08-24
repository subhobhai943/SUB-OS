#include <drivers/canvas.h>
#include <lib/font8x8.h>
#include <drivers/fb.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

static uint32_t* back_buffer = NULL;
static uint32_t screen_width = 320;
static uint32_t screen_height = 200;


void canvas_init(void) {
    const fb_info_t* fb = fb_get_info();
    if (fb && fb->width > 0 && fb->height > 0) {
        screen_width = fb->width;
        screen_height = fb->height;
    }
    back_buffer = (uint32_t*)kzalloc(screen_width * screen_height * sizeof(uint32_t));
    printk(KERN_INFO "CANVAS: 2D Graphics Canvas & Rasterizer online (%ux%u 32bpp)\n",
           screen_width, screen_height);
}

void canvas_clear(uint32_t color) {
    if (!back_buffer) return;
    for (size_t i = 0; i < screen_width * screen_height; i++) {
        back_buffer[i] = color;
    }
}

void canvas_draw_pixel(int x, int y, uint32_t color) {
    if (!back_buffer || x < 0 || x >= (int)screen_width || y < 0 || y >= (int)screen_height) return;
    back_buffer[y * screen_width + x] = color;
}

void canvas_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;

    while (1) {
        canvas_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy)  { err += dx; y0 += sy; }
    }
}

void canvas_draw_rect(int x, int y, int w, int h, uint32_t color, bool filled) {
    if (w <= 0 || h <= 0) return;
    if (filled) {
        for (int j = y; j < y + h; j++) {
            for (int i = x; i < x + w; i++) {
                canvas_draw_pixel(i, j, color);
            }
        }
    } else {
        canvas_draw_line(x, y, x + w - 1, y, color);
        canvas_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
        canvas_draw_line(x, y, x, y + h - 1, color);
        canvas_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    }
}

void canvas_draw_circle(int cx, int cy, int r, uint32_t color, bool filled) {
    int x = r;
    int y = 0;
    int err = 0;

    while (x >= y) {
        if (filled) {
            canvas_draw_line(cx - x, cy + y, cx + x, cy + y, color);
            canvas_draw_line(cx - x, cy - y, cx + x, cy - y, color);
            canvas_draw_line(cx - y, cy + x, cx + y, cy + x, color);
            canvas_draw_line(cx - y, cy - x, cx + y, cy - x, color);
        } else {
            canvas_draw_pixel(cx + x, cy + y, color);
            canvas_draw_pixel(cx + y, cy + x, color);
            canvas_draw_pixel(cx - y, cy + x, color);
            canvas_draw_pixel(cx - x, cy + y, color);
            canvas_draw_pixel(cx - x, cy - y, color);
            canvas_draw_pixel(cx - y, cy - x, color);
            canvas_draw_pixel(cx + y, cy - x, color);
            canvas_draw_pixel(cx + x, cy - y, color);
        }
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

void canvas_draw_char(int x, int y, char c, uint32_t color, uint32_t bg_color) {
    if (c < 32 || c > 126) c = ' ';
    const uint8_t* glyph = font8x8_glyph(c);

    for (int row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (line & (1 << (7 - col))) {
                canvas_draw_pixel(x + col, y + row, color);
            } else if (bg_color != 0) {
                canvas_draw_pixel(x + col, y + row, bg_color);
            }
        }
    }
}

void canvas_draw_string(int x, int y, const char* str, uint32_t color, uint32_t bg_color) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        canvas_draw_char(cur_x, y, *str, color, bg_color);
        cur_x += 8;
        str++;
    }
}

void canvas_render_desktop_mockup(void) {
    // 1. Wallpaper (Navy Gradient)
    canvas_clear(COLOR_NAVY);

    // 2. Top Banner / Desktop Header
    canvas_draw_rect(0, 0, screen_width, 16, COLOR_TITLE_BAR, true);
    canvas_draw_string(8, 4, "SUB-OS Graphical Desktop Environment v0.2.0", COLOR_WHITE, 0);
    canvas_draw_string(screen_width - 80, 4, "13:45 UTC", COLOR_YELLOW, 0);

    // 3. Floating Terminal Window (Simulated Window Manager)
    int win_x = 24, win_y = 30, win_w = 270, win_h = 135;
    canvas_draw_rect(win_x, win_y, win_w, win_h, COLOR_DARK_GRAY, true);
    canvas_draw_rect(win_x, win_y, win_w, win_h, COLOR_LIGHT_GRAY, false);

    // Window Title Bar
    canvas_draw_rect(win_x, win_y, win_w, 14, COLOR_ACCENT, true);
    canvas_draw_string(win_x + 6, win_y + 3, "Terminal - root@sub-os:~", COLOR_WHITE, 0);
    canvas_draw_circle(win_x + win_w - 8, win_y + 7, 3, COLOR_RED, true);
    canvas_draw_circle(win_x + win_w - 18, win_y + 7, 3, COLOR_YELLOW, true);
    canvas_draw_circle(win_x + win_w - 28, win_y + 7, 3, COLOR_GREEN, true);

    // Window Content
    canvas_draw_string(win_x + 8, win_y + 22, "Welcome to SUB-OS Kernel 0.2.0", COLOR_GREEN, 0);
    canvas_draw_string(win_x + 8, win_y + 36, "Tasks: 16 running | Memory: 122MB Free", COLOR_WHITE, 0);
    canvas_draw_string(win_x + 8, win_y + 50, "Services: HTTPD, SSHD, NetFilter active", COLOR_YELLOW, 0);
    canvas_draw_string(win_x + 8, win_y + 68, "sub-os:/> neofetch", COLOR_CYAN, 0);
    canvas_draw_string(win_x + 8, win_y + 82, "OS: SUB-OS (x86_64 Long Mode)", COLOR_WHITE, 0);
    canvas_draw_string(win_x + 8, win_y + 96, "Kernel: 0.2.0-lts Modular Monolithic", COLOR_WHITE, 0);
    canvas_draw_string(win_x + 8, win_y + 112, "sub-os:/> _", COLOR_GREEN, 0);

    // 4. Bottom Taskbar
    canvas_draw_rect(0, screen_height - 18, screen_width, 18, COLOR_TITLE_BAR, true);
    canvas_draw_rect(4, screen_height - 15, 45, 12, COLOR_ACCENT, true);
    canvas_draw_string(8, screen_height - 13, "Start", COLOR_WHITE, 0);
    canvas_draw_string(60, screen_height - 13, "[Terminal]", COLOR_LIGHT_GRAY, 0);
    canvas_draw_string(140, screen_height - 13, "[Nano]", COLOR_LIGHT_GRAY, 0);
    canvas_draw_string(195, screen_height - 13, "[HTTPD: 80]", COLOR_GREEN, 0);
    canvas_draw_string(screen_width - 45, screen_height - 13, "[OK]", COLOR_YELLOW, 0);

    canvas_present();
}

void canvas_present(void) {
    if (!back_buffer) return;
    const fb_info_t* fb = fb_get_info();
    if (fb && fb->address != 0) {
        uint32_t* hw_fb = (uint32_t*)fb->address;
        memcpy(hw_fb, back_buffer, screen_width * screen_height * sizeof(uint32_t));
    }
}
