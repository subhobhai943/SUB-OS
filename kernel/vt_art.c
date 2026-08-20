// ANSI Virtual Terminal Graphics, Color Palette & Telemetry Visualizer
#include <kernel/vt_art.h>
#include <kernel/printk.h>

void vt_show_palette_256(void) {
    printk(ANSI_BRIGHT_CYAN "=== 16-Color Standard & High-Intensity ANSI Palette ===\n" ANSI_RESET);
    for (int i = 0; i < 8; i++) {
        printk("\x1b[48;5;%dm  %2d  \x1b[0m", i, i);
    }
    printk("\n");
    for (int i = 8; i < 16; i++) {
        printk("\x1b[48;5;%dm  %2d  \x1b[0m", i, i);
    }
    printk("\n\n");

    printk(ANSI_BRIGHT_CYAN "=== 216-Color RGB Cube (6x6x6) ===\n" ANSI_RESET);
    for (int g = 0; g < 6; g++) {
        for (int r = 0; r < 6; r++) {
            for (int b = 0; b < 6; b++) {
                int code = 16 + (r * 36) + (g * 6) + b;
                printk("\x1b[48;5;%dm \x1b[0m", code);
            }
            printk(" ");
        }
        printk("\n");
    }
    printk("\n");

    printk(ANSI_BRIGHT_CYAN "=== 24-Step Grayscale Ramp ===\n" ANSI_RESET);
    for (int i = 232; i <= 255; i++) {
        printk("\x1b[48;5;%dm  \x1b[0m", i);
    }
    printk("\n\n");
}

static void draw_progress_bar(const char* label, uint32_t percent, const char* color_code) {
    printk("%-8s [", label);
    uint32_t filled = (percent * 30) / 100;
    if (filled > 30) filled = 30;

    printk("%s", color_code);
    for (uint32_t i = 0; i < filled; i++) {
        printk("█");
    }
    printk(ANSI_RESET);
    for (uint32_t i = filled; i < 30; i++) {
        printk("░");
    }
    printk("] %3u%%\n", percent);
}

void vt_show_load_bars(uint32_t cpu_percent, uint32_t mem_percent, uint32_t disk_percent) {
    printk(ANSI_BRIGHT_CYAN "=== Live System Load & Resource Utilization ===\n" ANSI_RESET);
    draw_progress_bar("CPU Core", cpu_percent, ANSI_BRIGHT_GREEN);
    draw_progress_bar("RAM Heap", mem_percent, ANSI_BRIGHT_YELLOW);
    draw_progress_bar("VFS Disk", disk_percent, ANSI_BRIGHT_CYAN);
    printk("\n");
}

void vt_show_logo_banner(void) {
    printk(ANSI_BRIGHT_CYAN
        "  ____  _   _ ____       ___  ____  \n"
        " / ___|| | | | __ )     / _ \\/ ___| \n"
        " \\___ \\| | | |  _ \\ ___| | | \\___ \\ \n"
        "  ___) | |_| | |_) |___| |_| |___) |\n"
        " |____/ \\___/|____/     \\___/|____/ \n"
        ANSI_RESET);
    printk(ANSI_BRIGHT_MAGENTA "  Modular Monolithic Linux-Compatible Hybrid Kernel\n\n" ANSI_RESET);
}
