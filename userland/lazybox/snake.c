// Ultra-Smooth, Responsive Classic Snake Game for SUB-OS
// Features Flicker-Free Double Buffering, Multi-Input (PS/2 & Serial), Sound Effects & Speed Control

#include <userland/snake.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/speaker.h>
#include <drivers/tty.h>
#include <arch/x86_64/pit.h>
#include <crypto/crypto.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define BOARD_WIDTH   32
#define BOARD_HEIGHT  16
#define MAX_SNAKE_LEN 256

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} direction_t;

typedef struct {
    int x;
    int y;
} point_t;

static point_t snake[MAX_SNAKE_LEN];
static int snake_len = 4;
static direction_t snake_dir = DIR_RIGHT;
static point_t food;
static int score = 0;
static int high_score = 0;
static bool game_over = false;
static bool game_paused = false;
static int speed_ms = 75; // Default tick interval

static void spawn_food(void) {
    bool valid = false;
    while (!valid) {
        food.x = 1 + (int)(prng_rand32() % (BOARD_WIDTH - 2));
        food.y = 1 + (int)(prng_rand32() % (BOARD_HEIGHT - 2));
        valid = true;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == food.x && snake[i].y == food.y) {
                valid = false;
                break;
            }
        }
    }
}

static void snake_init(void) {
    snake_len = 4;
    snake_dir = DIR_RIGHT;
    score = 0;
    game_over = false;
    game_paused = false;
    speed_ms = 75;

    int start_x = BOARD_WIDTH / 4;
    int start_y = BOARD_HEIGHT / 2;
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = start_x - i;
        snake[i].y = start_y;
    }

    spawn_food();
}

// Single-buffer atomic render eliminating all terminal flicker
static void snake_render(void) {
    char frame[4096];
    int offset = 0;

    // Reset cursor to top-left and hide cursor
    offset += snprintf(frame + offset, sizeof(frame) - offset, "\033[H\033[?25l");

    // Title & Score Header
    offset += snprintf(frame + offset, sizeof(frame) - offset,
        ANSI_BRIGHT_CYAN "╔══════════════════════════════════════════════╗\n" ANSI_RESET
        ANSI_BRIGHT_CYAN "║" ANSI_BRIGHT_YELLOW "   🐍 SUB-OS Classic Snake Game (ANSI)        " ANSI_BRIGHT_CYAN "║\n" ANSI_RESET
        ANSI_BRIGHT_CYAN "║" ANSI_WHITE " Score: " ANSI_BRIGHT_GREEN "%-4d" ANSI_WHITE " High: " ANSI_BRIGHT_YELLOW "%-4d" ANSI_WHITE " Speed: " ANSI_BRIGHT_CYAN "%-3dms " ANSI_WHITE "Len: " ANSI_BRIGHT_MAGENTA "%-3d" ANSI_BRIGHT_CYAN "║\n" ANSI_RESET
        ANSI_BRIGHT_CYAN "║" ANSI_BRIGHT_BLACK " Controls: WASD/Arrows, P:Pause, +/-:Speed, Q:Quit " ANSI_BRIGHT_CYAN "║\n" ANSI_RESET
        ANSI_BRIGHT_CYAN "╠══════════════════════════════════════════════╣\n" ANSI_RESET,
        score, high_score, speed_ms, snake_len);

    // Build Board Matrix
    char board[BOARD_HEIGHT][BOARD_WIDTH + 1];
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (y == 0 || y == BOARD_HEIGHT - 1 || x == 0 || x == BOARD_WIDTH - 1) {
                board[y][x] = '#';
            } else {
                board[y][x] = ' ';
            }
        }
        board[y][BOARD_WIDTH] = '\0';
    }

    // Place Food
    if (food.y >= 0 && food.y < BOARD_HEIGHT && food.x >= 0 && food.x < BOARD_WIDTH) {
        board[food.y][food.x] = '*';
    }

    // Place Snake Body
    for (int i = 1; i < snake_len; i++) {
        if (snake[i].y >= 0 && snake[i].y < BOARD_HEIGHT && snake[i].x >= 0 && snake[i].x < BOARD_WIDTH) {
            board[snake[i].y][snake[i].x] = 'o';
        }
    }

    // Place Snake Head
    if (snake[0].y >= 0 && snake[0].y < BOARD_HEIGHT && snake[0].x >= 0 && snake[0].x < BOARD_WIDTH) {
        board[snake[0].y][snake[0].x] = 'O';
    }

    // Render Grid
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        offset += snprintf(frame + offset, sizeof(frame) - offset, ANSI_BRIGHT_CYAN "║  " ANSI_RESET);
        for (int x = 0; x < BOARD_WIDTH; x++) {
            char c = board[y][x];
            if (c == '#') {
                offset += snprintf(frame + offset, sizeof(frame) - offset, ANSI_BRIGHT_BLUE "█" ANSI_RESET);
            } else if (c == 'O') {
                offset += snprintf(frame + offset, sizeof(frame) - offset, ANSI_BRIGHT_GREEN "O" ANSI_RESET);
            } else if (c == 'o') {
                offset += snprintf(frame + offset, sizeof(frame) - offset, ANSI_GREEN "o" ANSI_RESET);
            } else if (c == '*') {
                offset += snprintf(frame + offset, sizeof(frame) - offset, ANSI_BRIGHT_RED "★" ANSI_RESET);
            } else {
                offset += snprintf(frame + offset, sizeof(frame) - offset, " ");
            }
        }
        offset += snprintf(frame + offset, sizeof(frame) - offset, ANSI_BRIGHT_CYAN "            ║\n" ANSI_RESET);
    }

    // Footer
    if (game_paused) {
        offset += snprintf(frame + offset, sizeof(frame) - offset,
            ANSI_BRIGHT_CYAN "║" ANSI_BRIGHT_YELLOW "            *** GAME PAUSED ***               " ANSI_BRIGHT_CYAN "║\n" ANSI_RESET);
    } else {
        offset += snprintf(frame + offset, sizeof(frame) - offset,
            ANSI_BRIGHT_CYAN "╚══════════════════════════════════════════════╝\n" ANSI_RESET);
    }

    printk("%s", frame);
}

static void snake_step(void) {
    if (game_paused) return;

    point_t next_head = snake[0];

    switch (snake_dir) {
        case DIR_UP:    next_head.y--; break;
        case DIR_DOWN:  next_head.y++; break;
        case DIR_LEFT:  next_head.x--; break;
        case DIR_RIGHT: next_head.x++; break;
    }

    // Wall collision
    if (next_head.x <= 0 || next_head.x >= BOARD_WIDTH - 1 ||
        next_head.y <= 0 || next_head.y >= BOARD_HEIGHT - 1) {
        game_over = true;
        #if defined(__x86_64__)
        speaker_beep(220, 150);
        #endif
        return;
    }

    // Self collision
    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == next_head.x && snake[i].y == next_head.y) {
            game_over = true;
            #if defined(__x86_64__)
            speaker_beep(220, 150);
            #endif
            return;
        }
    }

    // Food collision
    bool ate_food = (next_head.x == food.x && next_head.y == food.y);

    if (ate_food) {
        score += 10;
        if (score > high_score) high_score = score;
        if (snake_len < MAX_SNAKE_LEN - 1) {
            snake_len++;
        }
        #if defined(__x86_64__)
        speaker_beep(1200, 30);
        #endif
        // Auto speed up slightly as score rises
        if (speed_ms > 40 && (score % 30 == 0)) {
            speed_ms -= 5;
        }
        spawn_food();
    }

    // Shift body
    for (int i = snake_len - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0] = next_head;
}

// Unified non-blocking input reader from both PS/2 Keyboard and Serial COM1
static int poll_input_char(void) {
    if (keyboard_has_key()) {
        uint16_t k = keyboard_get_key();
        if (k == KEY_UP) return 'w';
        if (k == KEY_DOWN) return 's';
        if (k == KEY_LEFT) return 'a';
        if (k == KEY_RIGHT) return 'd';
        return (int)(k & 0xFF);
    }
    if (serial_received()) {
        char c = serial_read_char();
        if (c == 27) { // Possible ANSI escape sequence \033[A
            #if defined(__x86_64__)
            pit_sleep(2);
            #endif
            if (serial_received()) {
                char c2 = serial_read_char();
                if (c2 == '[' && serial_received()) {
                    char c3 = serial_read_char();
                    if (c3 == 'A') return 'w';
                    if (c3 == 'B') return 's';
                    if (c3 == 'C') return 'd';
                    if (c3 == 'D') return 'a';
                }
            }
            return 27; // Bare ESC
        }
        return (int)c;
    }
    return -1;
}

int applet_snake(int argc, char** argv) {
    bool demo_mode = false;
    if (argc >= 2 && (strcmp(argv[1], "--demo") == 0 || strcmp(argv[1], "-d") == 0)) {
        demo_mode = true;
    }

    snake_init();

    if (demo_mode) {
        printk(ANSI_BRIGHT_CYAN "Running Snake Game Simulation Demo...\n" ANSI_RESET);
        for (int step = 0; step < 20; step++) {
            if (food.x > snake[0].x && snake_dir != DIR_LEFT) snake_dir = DIR_RIGHT;
            else if (food.x < snake[0].x && snake_dir != DIR_RIGHT) snake_dir = DIR_LEFT;
            else if (food.y > snake[0].y && snake_dir != DIR_UP) snake_dir = DIR_DOWN;
            else if (food.y < snake[0].y && snake_dir != DIR_DOWN) snake_dir = DIR_UP;

            snake_step();
            if (game_over) break;
        }
        snake_render();
        printk(ANSI_BRIGHT_GREEN "Snake Game Demo Finished! Final Score: %d\n" ANSI_RESET, score);
        return 0;
    }

    printk("\033[2J\033[H\033[?25l"); // Clear screen & hide cursor
    snake_render();

    int loop_counter = 0;
    while (!game_over) {
        // High-frequency sub-tick input polling for ultra-responsive controls
        int sub_ticks = speed_ms / 10;
        if (sub_ticks < 1) sub_ticks = 1;

        for (int st = 0; st < sub_ticks; st++) {
            int key = poll_input_char();
            if (key != -1) {
                if (key == 'q' || key == 'Q' || key == 27 || key == 3) {
                    game_over = true;
                    break;
                } else if (key == 'p' || key == 'P' || key == ' ') {
                    game_paused = !game_paused;
                    snake_render();
                } else if (key == '+' || key == '=') {
                    if (speed_ms > 25) speed_ms -= 10;
                    snake_render();
                } else if (key == '-' || key == '_') {
                    if (speed_ms < 200) speed_ms += 10;
                    snake_render();
                } else if ((key == 'w' || key == 'W' || key == 'k') && snake_dir != DIR_DOWN) {
                    snake_dir = DIR_UP;
                } else if ((key == 's' || key == 'S' || key == 'j') && snake_dir != DIR_UP) {
                    snake_dir = DIR_DOWN;
                } else if ((key == 'a' || key == 'A' || key == 'h') && snake_dir != DIR_RIGHT) {
                    snake_dir = DIR_LEFT;
                } else if ((key == 'd' || key == 'D' || key == 'l') && snake_dir != DIR_LEFT) {
                    snake_dir = DIR_RIGHT;
                }
            }
            #if defined(__x86_64__)
            pit_sleep(10);
            #else
            for (volatile int d = 0; d < 30000; d++);
            #endif
        }

        if (game_over) break;

        snake_step();
        snake_render();

        loop_counter++;
        if (loop_counter > 2000) break; // Safety timeout for automated headless environments
    }

    // Restore cursor
    printk("\033[?25h");
    tty_set_cursor(BOARD_HEIGHT + 6, 0);

    if (score >= high_score && score > 0) {
        printk(ANSI_BRIGHT_YELLOW "\n🏆 NEW HIGH SCORE: %d! 🎉\n" ANSI_RESET, score);
    }
    printk(ANSI_BRIGHT_RED "\n💥 GAME OVER! Final Score: %d (Snake Length: %d)\n" ANSI_RESET, score, snake_len);

    return 0;
}
