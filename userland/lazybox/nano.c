#include <userland/nano.h>
#include <fs/vfs.h>
#include <drivers/keyboard.h>
#include <drivers/tty.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define NANO_MAX_LINES 512
#define NANO_LINE_LEN  128
#define NANO_SCREEN_ROWS 21

static char nano_lines[NANO_MAX_LINES][NANO_LINE_LEN];
static uint32_t nano_line_count = 1;
static int nano_cursor_row = 0;
static int nano_cursor_col = 0;
static int nano_top_row = 0;
static bool nano_modified = false;
static char nano_filename[128];
static char nano_status[80];
static char nano_cut_buffer[NANO_LINE_LEN];
static bool nano_has_cut = false;

static void nano_render(void) {
    tty_clear();

    // 1. Title bar (Inverted Color)
    printk(ANSI_INVERT "  GNU nano 2.0.0             File: %-32s %s" ANSI_RESET "\n",
           nano_filename[0] ? nano_filename : "New Buffer",
           nano_modified ? "[Modified]" : "          ");

    // 2. Viewport Lines (Rows 1 to 21)
    for (int r = 0; r < NANO_SCREEN_ROWS; r++) {
        int line_idx = nano_top_row + r;
        if (line_idx < (int)nano_line_count) {
            printk("%s\n", nano_lines[line_idx]);
        } else {
            printk("~\n");
        }
    }

    // 3. Status Message Bar
    printk(ANSI_BRIGHT_CYAN "%-78s" ANSI_RESET "\n", nano_status);

    // 4. Shortcut Cheatsheet Footer
    printk(ANSI_INVERT "^G Get Help  ^O WriteOut  ^W Where Is  ^K Cut Line  ^U Paste  ^X Exit" ANSI_RESET "\n");

    // 5. Position Hardware Cursor
    int scr_row = (nano_cursor_row - nano_top_row) + 2;
    int scr_col = nano_cursor_col + 1;
    tty_set_cursor(scr_row - 1, scr_col - 1);
}

static void nano_save(void) {
    if (!nano_filename[0]) {
        strcpy(nano_status, "[ Error: No filename specified ]");
        return;
    }

    int fd = vfs_open(nano_filename, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        strcpy(nano_status, "[ Error opening file for writing ]");
        return;
    }

    size_t total_bytes = 0;
    for (uint32_t i = 0; i < nano_line_count; i++) {
        size_t len = strlen(nano_lines[i]);
        if (len > 0) {
            vfs_write(fd, nano_lines[i], len);
            total_bytes += len;
        }
        if (i + 1 < nano_line_count || len > 0) {
            vfs_write(fd, "\n", 1);
            total_bytes += 1;
        }
    }
    vfs_close(fd);

    nano_modified = false;
    sprintf(nano_status, "[ Wrote %llu bytes to %s ]", (uint64_t)total_bytes, nano_filename);
}

static void nano_load(const char* path) {
    memset(nano_lines, 0, sizeof(nano_lines));
    nano_line_count = 0;
    nano_cursor_row = 0;
    nano_cursor_col = 0;
    nano_top_row = 0;
    nano_modified = false;
    nano_status[0] = '\0';

    strncpy(nano_filename, path, sizeof(nano_filename) - 1);

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        nano_line_count = 1;
        nano_lines[0][0] = '\0';
        sprintf(nano_status, "[ New File: %s ]", path);
        return;
    }

    char buf[512];
    ssize_t bytes;
    int cur_line = 0;
    int cur_col = 0;

    while ((bytes = vfs_read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                nano_lines[cur_line][cur_col] = '\0';
                cur_line++;
                cur_col = 0;
                if (cur_line >= NANO_MAX_LINES) break;
            } else {
                if (cur_col < NANO_LINE_LEN - 1) {
                    nano_lines[cur_line][cur_col++] = c;
                }
            }
        }
        if (cur_line >= NANO_MAX_LINES) break;
    }
    vfs_close(fd);

    if (cur_col > 0 || cur_line == 0) {
        nano_lines[cur_line][cur_col] = '\0';
        cur_line++;
    }
    nano_line_count = cur_line > 0 ? cur_line : 1;
    sprintf(nano_status, "[ Read %llu line(s) from %s ]", (uint64_t)nano_line_count, path);
}

int nano_main(int argc, char** argv) {
    if (argc < 2) {
        nano_load("");
        strcpy(nano_status, "[ New Buffer - Use ^O to save ]");
    } else {
        nano_load(argv[1]);
    }

    bool running = true;

    while (running) {
        // Adjust viewport scrolling
        if (nano_cursor_row < nano_top_row) {
            nano_top_row = nano_cursor_row;
        } else if (nano_cursor_row >= nano_top_row + NANO_SCREEN_ROWS) {
            nano_top_row = nano_cursor_row - NANO_SCREEN_ROWS + 1;
        }

        // Clamp cursor column
        int line_len = (int)strlen(nano_lines[nano_cursor_row]);
        if (nano_cursor_col > line_len) {
            nano_cursor_col = line_len;
        }
        if (nano_cursor_col < 0) nano_cursor_col = 0;

        nano_render();

        uint16_t key = keyboard_get_key();

        switch (key) {
            case 0x18: // Ctrl+X -> Exit
                running = false;
                break;

            case 0x0F: // Ctrl+O -> Write Out / Save
                nano_save();
                break;

            case 0x0B: // Ctrl+K -> Cut line
                if (nano_line_count > 0) {
                    strcpy(nano_cut_buffer, nano_lines[nano_cursor_row]);
                    nano_has_cut = true;
                    for (uint32_t i = nano_cursor_row; i + 1 < nano_line_count; i++) {
                        strcpy(nano_lines[i], nano_lines[i + 1]);
                    }
                    if (nano_line_count > 1) {
                        nano_line_count--;
                    } else {
                        nano_lines[0][0] = '\0';
                    }
                    nano_modified = true;
                    strcpy(nano_status, "[ Cut 1 line ]");
                }
                break;

            case 0x15: // Ctrl+U -> Uncut / Paste
                if (nano_has_cut && nano_line_count < NANO_MAX_LINES) {
                    for (int i = (int)nano_line_count; i > nano_cursor_row; i--) {
                        strcpy(nano_lines[i], nano_lines[i - 1]);
                    }
                    strcpy(nano_lines[nano_cursor_row], nano_cut_buffer);
                    nano_line_count++;
                    nano_modified = true;
                    strcpy(nano_status, "[ Pasted 1 line ]");
                }
                break;

            case 0x07: // Ctrl+G -> Help
                strcpy(nano_status, "[ Help: ^O=Save, ^X=Exit, ^K=Cut, ^U=Paste, Arrows=Move ]");
                break;

            case KEY_UP:
                if (nano_cursor_row > 0) {
                    nano_cursor_row--;
                }
                break;

            case KEY_DOWN:
                if (nano_cursor_row + 1 < (int)nano_line_count) {
                    nano_cursor_row++;
                }
                break;

            case KEY_LEFT:
                if (nano_cursor_col > 0) {
                    nano_cursor_col--;
                } else if (nano_cursor_row > 0) {
                    nano_cursor_row--;
                    nano_cursor_col = strlen(nano_lines[nano_cursor_row]);
                }
                break;

            case KEY_RIGHT:
                if (nano_cursor_col < line_len) {
                    nano_cursor_col++;
                } else if (nano_cursor_row + 1 < (int)nano_line_count) {
                    nano_cursor_row++;
                    nano_cursor_col = 0;
                }
                break;

            case KEY_HOME:
                nano_cursor_col = 0;
                break;

            case KEY_END:
                nano_cursor_col = line_len;
                break;

            case KEY_PAGE_UP:
                nano_cursor_row = nano_cursor_row >= NANO_SCREEN_ROWS ? nano_cursor_row - NANO_SCREEN_ROWS : 0;
                break;

            case KEY_PAGE_DOWN:
                nano_cursor_row = (nano_cursor_row + NANO_SCREEN_ROWS < (int)nano_line_count) ?
                                  nano_cursor_row + NANO_SCREEN_ROWS : (int)nano_line_count - 1;
                break;

            case '\n': // Enter -> Split line
                if (nano_line_count < NANO_MAX_LINES) {
                    for (int i = (int)nano_line_count; i > nano_cursor_row + 1; i--) {
                        strcpy(nano_lines[i], nano_lines[i - 1]);
                    }
                    strcpy(nano_lines[nano_cursor_row + 1], &nano_lines[nano_cursor_row][nano_cursor_col]);
                    nano_lines[nano_cursor_row][nano_cursor_col] = '\0';
                    nano_line_count++;
                    nano_cursor_row++;
                    nano_cursor_col = 0;
                    nano_modified = true;
                }
                break;

            case '\b': // Backspace
                if (nano_cursor_col > 0) {
                    memmove(&nano_lines[nano_cursor_row][nano_cursor_col - 1],
                            &nano_lines[nano_cursor_row][nano_cursor_col],
                            strlen(&nano_lines[nano_cursor_row][nano_cursor_col]) + 1);
                    nano_cursor_col--;
                    nano_modified = true;
                } else if (nano_cursor_row > 0) {
                    int prev_len = (int)strlen(nano_lines[nano_cursor_row - 1]);
                    if (prev_len + line_len < NANO_LINE_LEN - 1) {
                        strcat(nano_lines[nano_cursor_row - 1], nano_lines[nano_cursor_row]);
                        for (uint32_t i = nano_cursor_row; i + 1 < nano_line_count; i++) {
                            strcpy(nano_lines[i], nano_lines[i + 1]);
                        }
                        nano_line_count--;
                        nano_cursor_row--;
                        nano_cursor_col = prev_len;
                        nano_modified = true;
                    }
                }
                break;

            default:
                if (key >= 32 && key <= 126) {
                    char c = (char)key;
                    if (line_len < NANO_LINE_LEN - 2) {
                        memmove(&nano_lines[nano_cursor_row][nano_cursor_col + 1],
                                &nano_lines[nano_cursor_row][nano_cursor_col],
                                strlen(&nano_lines[nano_cursor_row][nano_cursor_col]) + 1);
                        nano_lines[nano_cursor_row][nano_cursor_col] = c;
                        nano_cursor_col++;
                        nano_modified = true;
                    }
                }
                break;
        }
    }

    tty_clear();
    return 0;
}
