// Interactive Classic Snake Game for SUB-OS
// Features ANSI graphics, real-time keyboard control, dynamic scoring & demo mode

#include <userland/snake.h>
#include <drivers/keyboard.h>
#include <drivers/tty.h>
#include <arch/x86_64/pit.h>
#include <crypto/crypto.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define BOARD_WIDTH  32
#define BOARD_HEIGHT 16
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
static bool game_over = false;

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

    int start_x = BOARD_WIDTH / 4;
    int start_y = BOARD_HEIGHT / 2;
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = start_x - i;
        snake[i].y = start_y;
    }

    spawn_food();
}

static void snake_render(void) {
    tty_set_cursor(0, 0);

    // Title & Score Header
    printk(ANSI_BRIGHT_CYAN "╔══════════════════════════════════════════════╗\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "║" ANSI_BRIGHT_YELLOW "   🐍 SUB-OS Classic Snake Game (ANSI)        " ANSI_BRIGHT_CYAN "║\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "║" ANSI_WHITE "   Score: " ANSI_BRIGHT_GREEN "%-4d" ANSI_WHITE "   Length: " ANSI_BRIGHT_GREEN "%-3d" ANSI_WHITE "   Controls: WASD/Q " ANSI_BRIGHT_CYAN "║\n" ANSI_RESET, score, snake_len);
    printk(ANSI_BRIGHT_CYAN "╠══════════════════════════════════════════════╣\n" ANSI_RESET);

    // Render Board Grid
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

    // Draw Grid with Colors
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        printk(ANSI_BRIGHT_CYAN "║  " ANSI_RESET);
        for (int x = 0; x < BOARD_WIDTH; x++) {
            char c = board[y][x];
            if (c == '#') {
                printk(ANSI_BRIGHT_BLUE "█" ANSI_RESET);
            } else if (c == 'O') {
                printk(ANSI_BRIGHT_GREEN "O" ANSI_RESET);
            } else if (c == 'o') {
                printk(ANSI_GREEN "o" ANSI_RESET);
            } else if (c == '*') {
                printk(ANSI_BRIGHT_RED "★" ANSI_RESET);
            } else {
                printk(" ");
            }
        }
        printk(ANSI_BRIGHT_CYAN "            ║\n" ANSI_RESET);
    }

    printk(ANSI_BRIGHT_CYAN "╚══════════════════════════════════════════════╝\n" ANSI_RESET);
}

static void snake_step(void) {
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
        return;
    }

    // Self collision
    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == next_head.x && snake[i].y == next_head.y) {
            game_over = true;
            return;
        }
    }

    // Food check
    bool ate_food = (next_head.x == food.x && next_head.y == food.y);

    if (ate_food) {
        score += 10;
        if (snake_len < MAX_SNAKE_LEN - 1) {
            snake_len++;
        }
        spawn_food();
    }

    // Shift body
    for (int i = snake_len - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0] = next_head;
}

int applet_snake(int argc, char** argv) {
    bool demo_mode = false;
    if (argc >= 2 && (strcmp(argv[1], "--demo") == 0 || strcmp(argv[1], "-d") == 0)) {
        demo_mode = true;
    }

    snake_init();

    if (demo_mode) {
        printk(ANSI_BRIGHT_CYAN "Running Snake Game Simulation Demo...\n" ANSI_RESET);
        for (int step = 0; step < 15; step++) {
            // Simple autonomous AI chasing food
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

    printk("\033[2J\033[H");
    snake_render();

    int loop_counter = 0;
    while (!game_over) {
        if (keyboard_has_key()) {
            uint16_t key = keyboard_get_key();
            char c = (char)(key & 0xFF);

            if (c == 'q' || c == 'Q' || c == 27) { // Q or ESC
                break;
            } else if ((c == 'w' || c == 'W' || key == KEY_UP) && snake_dir != DIR_DOWN) {
                snake_dir = DIR_UP;
            } else if ((c == 's' || c == 'S' || key == KEY_DOWN) && snake_dir != DIR_UP) {
                snake_dir = DIR_DOWN;
            } else if ((c == 'a' || c == 'A' || key == KEY_LEFT) && snake_dir != DIR_RIGHT) {
                snake_dir = DIR_LEFT;
            } else if ((c == 'd' || c == 'D' || key == KEY_RIGHT) && snake_dir != DIR_LEFT) {
                snake_dir = DIR_RIGHT;
            }
        }

        snake_step();
        snake_render();

        #if defined(__x86_64__)
        pit_sleep(70);
        #else
        for (volatile int d = 0; d < 200000; d++);
        #endif

        loop_counter++;
        if (loop_counter > 500) break; // Avoid infinite loop in unattended tests
    }

    tty_set_cursor(BOARD_HEIGHT + 5, 0);
    if (game_over) {
        printk(ANSI_BRIGHT_RED "\n💥 GAME OVER! Final Score: %d (Length: %d)\n" ANSI_RESET, score, snake_len);
    } else {
        printk(ANSI_YELLOW "\nGame exited. Final Score: %d\n" ANSI_RESET, score);
    }

    return 0;
}
