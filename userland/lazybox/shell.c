#include <userland/shell.h>
#include <userland/lazybox.h>
#include <fs/vfs.h>
#include <drivers/tty.h>
#include <drivers/keyboard.h>
#include <drivers/speaker.h>
#include <arch/arch.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/printf.h>

#define HISTORY_SIZE 16

static char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;
static int history_head = 0;

static const char* shell_builtins[] = {
    "lazybox", "nano", "ls", "cat", "touch", "mkdir", "pwd", "cd", "wc", "echo",
    "head", "tail", "stat", "cp", "grep", "hexdump", "tts", "alsamixer",
    "lsmod", "insmod", "rmmod", "md5sum", "sha256sum", "crc32", "rand",
    "certcheck", "capsh", "ipcs", "ifconfig", "ping", "arp", "dhclient",
    "nslookup", "hdparm", "lspci", "speaker", "mouse", "virtinfo",
    "io_uring_test", "uname", "free", "uptime", "dmesg", "ps", "top",
    "sleep", "reboot", "poweroff", "clear", "help", "neofetch", "calc",
    "matrix", "history", "tty", "beep", "shutdown", NULL
};

static void history_add(const char* cmd) {
    if (!cmd || cmd[0] == '\0') return;
    if (history_count > 0) {
        int last_idx = (history_head - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        if (strcmp(history[last_idx], cmd) == 0) return;
    }
    strncpy(history[history_head], cmd, MAX_COMMAND_LENGTH - 1);
    history[history_head][MAX_COMMAND_LENGTH - 1] = '\0';
    history_head = (history_head + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) history_count++;
}

static void redraw_line(const char* prompt, const char* buffer, size_t len, size_t cursor_pos) {
    printk("\r\033[K%s%s", prompt, buffer);
    if (cursor_pos < len) {
        size_t diff = len - cursor_pos;
        for (size_t i = 0; i < diff; i++) {
            printk("\033[D");
        }
    }
}

static void cmd_help(void) {
    printk(ANSI_BRIGHT_CYAN "================== SUB-OS Production Command Reference ==================\n" ANSI_RESET);
    printk(ANSI_YELLOW "  [File & Storage Tools]\n" ANSI_RESET);
    printk("    " ANSI_GREEN "nano [file]" ANSI_RESET "        - Full-screen visual interactive text editor\n");
    printk("    " ANSI_GREEN "ls [path]" ANSI_RESET "          - Directory list with type & inode numbers\n");
    printk("    " ANSI_GREEN "cat <file>" ANSI_RESET "         - Display file contents (supports /proc & /dev)\n");
    printk("    " ANSI_GREEN "touch <file>" ANSI_RESET "       - Create empty files in VFS\n");
    printk("    " ANSI_GREEN "mkdir <dir>" ANSI_RESET "        - Create directories in VFS\n");
    printk("    " ANSI_GREEN "cd [dir] / pwd" ANSI_RESET "     - Change / Print current working directory\n");
    printk("    " ANSI_GREEN "hdparm" ANSI_RESET "             - Inspect ATA hard disk parameters & sectors\n\n");
    printk(ANSI_YELLOW "  [Sound & Audio]\n" ANSI_RESET);
    printk("    " ANSI_GREEN "tts <text>" ANSI_RESET "         - Phonetic formant voice synthesizer\n");
    printk("    " ANSI_GREEN "alsamixer" ANSI_RESET "          - Sound architecture & audio card status\n\n");
    printk(ANSI_YELLOW "  [Kernel & Modules]\n" ANSI_RESET);
    printk("    " ANSI_GREEN "lsmod / insmod / rmmod" ANSI_RESET " - Manage dynamically loadable kernel modules\n\n");
    printk(ANSI_YELLOW "  [Cryptography & Security]\n" ANSI_RESET);
    printk("    " ANSI_GREEN "sha256sum <file>" ANSI_RESET "   - FIPS 180-4 SHA-256 secure hash\n");
    printk("    " ANSI_GREEN "md5sum <file>" ANSI_RESET "      - RFC 1321 MD5 message digest\n");
    printk("    " ANSI_GREEN "crc32 <text>" ANSI_RESET "       - IEEE 802.3 32-bit CRC calculation\n");
    printk("    " ANSI_GREEN "certcheck / capsh" ANSI_RESET "  - X.509 keyring & POSIX capabilities\n\n");
    printk(ANSI_YELLOW "  [Networking Suite]\n" ANSI_RESET);
    printk("    " ANSI_GREEN "ifconfig" ANSI_RESET "           - Ethernet eth0 configuration & RX/TX metrics\n");
    printk("    " ANSI_GREEN "ping <ip>" ANSI_RESET "          - Send ICMP echo requests with latency ms\n");
    printk("    " ANSI_GREEN "dhclient" ANSI_RESET "           - Request IP lease from DHCP server\n");
    printk("    " ANSI_GREEN "nslookup <host>" ANSI_RESET "    - Query DNS domain name server\n\n");
    printk(ANSI_YELLOW "  [System & Diagnostics]\n" ANSI_RESET);
    printk("    " ANSI_GREEN "neofetch" ANSI_RESET "           - System information & OS ASCII badge\n");
    printk("    " ANSI_GREEN "free / uptime" ANSI_RESET "      - Memory metrics and CPU uptime\n");
    printk("    " ANSI_GREEN "dmesg / ps / top" ANSI_RESET "   - Kernel boot log & process manager\n");
    printk("    " ANSI_GREEN "calc [A] [op] [B]" ANSI_RESET " - Arithmetic calculator (+, -, *, /, %%)\n");
    printk("    " ANSI_GREEN "matrix" ANSI_RESET "             - Digital rain screen animation\n");
    printk("    " ANSI_GREEN "reboot / shutdown" ANSI_RESET "   - Power management\n");
    printk(ANSI_BRIGHT_BLACK "Tip: Use TAB for autocomplete, Up/Down for command history.\n" ANSI_RESET);
}

static void cmd_neofetch(void) {
    uint64_t total = pmm_get_total_pages();
    uint64_t used = pmm_get_used_pages();
    uint64_t free = pmm_get_free_pages();
    uint64_t total_mb = (total * 4096) / (1024 * 1024);
    uint64_t used_mb = (used * 4096) / (1024 * 1024);

    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t mins = secs / 60;
    uint64_t hours = mins / 60;
    secs %= 60;
    mins %= 60;

    printk("\n");
    printk(ANSI_BRIGHT_CYAN "   _____ _    _ ____     " ANSI_BRIGHT_GREEN  "OS:      " ANSI_RESET "SUB-OS v0.2.0-lts (Modular Monolithic)\n");
    printk(ANSI_BRIGHT_CYAN "  / ____| |  | |  _ \\    " ANSI_BRIGHT_GREEN  "Arch:    " ANSI_RESET "%s\n", arch_get_name());
    printk(ANSI_BRIGHT_CYAN " | (___ | |  | | |_) |   " ANSI_BRIGHT_GREEN  "Kernel:  " ANSI_RESET "Modular Drivers + VFS + Crypto + Net\n");
#if defined(__x86_64__)
    const cpu_info_t* cpu = cpuid_get_info();
    printk(ANSI_BRIGHT_CYAN "  \\___ \\| |  | |  _ <    " ANSI_BRIGHT_GREEN  "CPU:     " ANSI_RESET "%s (%s)\n", cpu->model, cpu->vendor);
#elif defined(__aarch64__)
    const aarch64_cpu_info_t* cpu = aarch64_get_cpu_info();
    printk(ANSI_BRIGHT_CYAN "  \\___ \\| |  | |  _ <    " ANSI_BRIGHT_GREEN  "CPU:     " ANSI_RESET "%s (EL%u)\n", cpu->model_name, cpu->current_el);
#elif defined(__arm__) || defined(__armv8i__)
    const armv8i_cpu_info_t* cpu = armv8i_get_cpu_info();
    printk(ANSI_BRIGHT_CYAN "  \\___ \\| |  | |  _ <    " ANSI_BRIGHT_GREEN  "CPU:     " ANSI_RESET "%s (AArch32)\n", cpu->model_name);
#endif
    printk(ANSI_BRIGHT_CYAN "  ____) | |__| | |_) |   " ANSI_BRIGHT_GREEN  "Memory:  " ANSI_RESET "%llu MB / %llu MB (Free: %llu MB)\n", used_mb, total_mb, (free * 4096) / (1024 * 1024));
    printk(ANSI_BRIGHT_CYAN " |_____/ \\____/|____/    " ANSI_BRIGHT_GREEN  "Heap:    " ANSI_RESET "%llu KB used / %llu KB total\n", (uint64_t)(heap_get_used_bytes() / 1024), (uint64_t)((heap_get_used_bytes() + heap_get_free_bytes()) / 1024));
    printk(ANSI_BRIGHT_CYAN "                         " ANSI_BRIGHT_GREEN  "Uptime:  " ANSI_RESET "%02llu:%02llu:%02llu\n", hours, mins, secs);
    printk(ANSI_BRIGHT_CYAN "                         " ANSI_BRIGHT_GREEN  "TTY:     " ANSI_RESET "tty%d (Alt+F1-F4 to switch)\n", tty_get_current() + 1);
    printk(" \n");
    printk(ANSI_BLACK "\xdb\xdb" ANSI_RED "\xdb\xdb" ANSI_GREEN "\xdb\xdb" ANSI_YELLOW "\xdb\xdb" ANSI_BLUE "\xdb\xdb" ANSI_MAGENTA "\xdb\xdb" ANSI_CYAN "\xdb\xdb" ANSI_WHITE "\xdb\xdb" ANSI_RESET "\n\n");
}

static void cmd_calc(const char* args) {
    while (*args == ' ') args++;
    int64_t a = 0, b = 0;
    char op = 0;

    bool neg_a = false;
    if (*args == '-') { neg_a = true; args++; }
    while (*args >= '0' && *args <= '9') { a = a * 10 + (*args - '0'); args++; }
    if (neg_a) a = -a;

    while (*args == ' ') args++;
    if (*args) op = *args++;

    while (*args == ' ') args++;
    bool neg_b = false;
    if (*args == '-') { neg_b = true; args++; }
    while (*args >= '0' && *args <= '9') { b = b * 10 + (*args - '0'); args++; }
    if (neg_b) b = -b;

    int64_t res = 0;
    if (op == '+') res = a + b;
    else if (op == '-') res = a - b;
    else if (op == '*') res = a * b;
    else if (op == '/') {
        if (b == 0) { printk(ANSI_RED "Division by zero\n" ANSI_RESET); return; }
        res = a / b;
    } else if (op == '%') {
        if (b == 0) { printk(ANSI_RED "Modulo by zero\n" ANSI_RESET); return; }
        res = a % b;
    } else {
        printk("Usage: calc 15 + 27\n");
        return;
    }

    printk("%lld %c %lld = " ANSI_BRIGHT_GREEN "%lld\n" ANSI_RESET, a, op, b, res);
}

static void cmd_hexdump(const char* args) {
    uint64_t addr = 0x100000;
    uint64_t len = 64;

    while (*args == ' ') args++;
    if (*args) {
        if (args[0] == '0' && (args[1] == 'x' || args[1] == 'X')) args += 2;
        addr = 0;
        while ((*args >= '0' && *args <= '9') || (*args >= 'a' && *args <= 'f') || (*args >= 'A' && *args <= 'F')) {
            addr <<= 4;
            if (*args >= '0' && *args <= '9') addr |= (*args - '0');
            else if (*args >= 'a' && *args <= 'f') addr |= (*args - 'a' + 10);
            else if (*args >= 'A' && *args <= 'F') addr |= (*args - 'A' + 10);
            args++;
        }
        while (*args == ' ') args++;
        if (*args >= '0' && *args <= '9') {
            len = 0;
            while (*args >= '0' && *args <= '9') {
                len = len * 10 + (*args - '0');
                args++;
            }
        }
    }
    if (len > 512) len = 512;

    uint8_t* ptr = (uint8_t*)addr;
    printk("Hexdump of 0x%lx (%llu bytes):\n", addr, len);
    for (uint64_t i = 0; i < len; i += 16) {
        printk(ANSI_BRIGHT_BLACK "%016lx: " ANSI_RESET, addr + i);
        for (uint64_t j = 0; j < 16; j++) {
            if (i + j < len) printk("%02x ", ptr[i + j]);
            else printk("   ");
            if (j == 7) printk(" ");
        }
        printk(" |");
        for (uint64_t j = 0; j < 16; j++) {
            if (i + j < len) {
                uint8_t b = ptr[i + j];
                printk("%c", (b >= 32 && b <= 126) ? (char)b : '.');
            }
        }
        printk("|\n");
    }
}

static void cmd_matrix(void) {
    tty_clear();
    printk(ANSI_BRIGHT_GREEN "Press any key to exit Matrix mode...\n" ANSI_RESET);
    pit_sleep(600);
    tty_clear();

    int drops[TTY_WIDTH];
    for (int i = 0; i < TTY_WIDTH; i++) drops[i] = -(i % 25);
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*";
    int charset_len = sizeof(charset) - 1;
    uint32_t rand_seed = (uint32_t)pit_get_ticks();

    while (!keyboard_has_key()) {
        for (int x = 0; x < TTY_WIDTH; x += 2) {
            rand_seed = rand_seed * 1103515245 + 12345;
            int y = drops[x];
            if (y >= 0 && y < TTY_HEIGHT) {
                tty_set_cursor(y, x);
                printk("\033[92m%c\033[0m", charset[rand_seed % charset_len]);
            }
            if (y - 8 >= 0 && y - 8 < TTY_HEIGHT) {
                tty_set_cursor(y - 8, x);
                printk(" ");
            }
            drops[x]++;
            if (drops[x] - 8 >= TTY_HEIGHT) drops[x] = 0;
        }
        pit_sleep(40);
    }
    keyboard_get_key();
    tty_clear();
}

static void shell_process(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char* argv[16];
    int argc = 0;
    char* token = cmd_copy;

    while (*token) {
        while (*token == ' ') *token++ = '\0';
        if (*token == '\0') break;
        if (argc < 16) argv[argc++] = token;
        while (*token && *token != ' ') token++;
    }
    if (argc == 0) return;

    // 1. LazyBox Applets (ls, cat, touch, mkdir, cd, pwd, nano, etc.)
    if (lazybox_has_applet(argv[0])) {
        lazybox_run_applet(argv[0], argc, argv);
        return;
    }

    // 2. Builtins
    if (strcmp(argv[0], "help") == 0) {
        cmd_help();
    } else if (strcmp(argv[0], "neofetch") == 0 || strcmp(argv[0], "version") == 0) {
        cmd_neofetch();
    } else if (strcmp(argv[0], "calc") == 0) {
        cmd_calc(cmd + 4);
    } else if (strcmp(argv[0], "hexdump") == 0) {
        cmd_hexdump(cmd + 7);
    } else if (strcmp(argv[0], "matrix") == 0) {
        cmd_matrix();
    } else if (strcmp(argv[0], "beep") == 0) {
        speaker_beep(587, 100);
    } else if (strcmp(argv[0], "tty") == 0) {
        if (argc >= 2 && argv[1][0] >= '1' && argv[1][0] <= '4') {
            tty_switch(argv[1][0] - '1');
        } else {
            printk("Active TTY: tty%d (Switch with 'tty 1-4' or Alt+F1-F4)\n", tty_get_current() + 1);
        }
    } else if (strcmp(argv[0], "history") == 0) {
        for (int i = 0; i < history_count; i++) {
            int idx = (history_head - history_count + i + HISTORY_SIZE) % HISTORY_SIZE;
            printk("  %2d: %s\n", i + 1, history[idx]);
        }
    } else if (strcmp(argv[0], "reboot") == 0) {
        printk(ANSI_YELLOW "Rebooting...\n" ANSI_RESET);
        pit_sleep(200);
        outb(0x64, 0xFE);
    } else if (strcmp(argv[0], "shutdown") == 0 || strcmp(argv[0], "poweroff") == 0) {
        printk(ANSI_YELLOW "System halted.\n" ANSI_RESET);
        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        while (1) hlt();
    } else {
        printk(ANSI_RED "Unknown command: %s. Type 'help' or 'lazybox'.\n" ANSI_RESET, argv[0]);
    }
}

void shell_run(void) {
    char cmd_buffer[MAX_COMMAND_LENGTH];
    size_t cmd_len = 0;
    size_t cursor_pos = 0;
    int history_idx = -1;
    char prompt[128];

    printk(ANSI_BRIGHT_CYAN "\nWelcome to SUB-OS Modular Monolithic Linux Core!\n" ANSI_RESET);
    printk("Type '" ANSI_YELLOW "help" ANSI_RESET "', '" ANSI_YELLOW "lazybox" ANSI_RESET "', or '" ANSI_YELLOW "neofetch" ANSI_RESET "' to begin.\n\n");

    while (true) {
        const char* cwd = vfs_getcwd();
        snprintf(prompt, sizeof(prompt), ANSI_BRIGHT_GREEN "sub-os:" ANSI_BRIGHT_CYAN "%s" ANSI_BRIGHT_GREEN "> " ANSI_RESET, cwd);
        printk("%s", prompt);

        cmd_len = 0;
        cursor_pos = 0;
        cmd_buffer[0] = '\0';
        history_idx = -1;

        while (true) {
            uint16_t key = keyboard_get_key();

            if (key & KEY_SPECIAL_FLAG) {
                switch (key) {
                    case KEY_LEFT:
                        if (cursor_pos > 0) { cursor_pos--; printk("\033[D"); }
                        break;
                    case KEY_RIGHT:
                        if (cursor_pos < cmd_len) { cursor_pos++; printk("\033[C"); }
                        break;
                    case KEY_HOME:
                        while (cursor_pos > 0) { cursor_pos--; printk("\033[D"); }
                        break;
                    case KEY_END:
                        while (cursor_pos < cmd_len) { cursor_pos++; printk("\033[C"); }
                        break;
                    case KEY_DELETE:
                        if (cursor_pos < cmd_len) {
                            for (size_t i = cursor_pos; i < cmd_len - 1; i++) cmd_buffer[i] = cmd_buffer[i + 1];
                            cmd_len--;
                            cmd_buffer[cmd_len] = '\0';
                            redraw_line(prompt, cmd_buffer, cmd_len, cursor_pos);
                        }
                        break;
                    case KEY_UP:
                        if (history_count > 0) {
                            if (history_idx == -1) history_idx = history_count - 1;
                            else if (history_idx > 0) history_idx--;
                            int actual = (history_head - history_count + history_idx + HISTORY_SIZE) % HISTORY_SIZE;
                            strncpy(cmd_buffer, history[actual], MAX_COMMAND_LENGTH - 1);
                            cmd_buffer[MAX_COMMAND_LENGTH - 1] = '\0';
                            cmd_len = strlen(cmd_buffer);
                            cursor_pos = cmd_len;
                            redraw_line(prompt, cmd_buffer, cmd_len, cursor_pos);
                        }
                        break;
                    case KEY_DOWN:
                        if (history_idx != -1) {
                            if (history_idx < history_count - 1) {
                                history_idx++;
                                int actual = (history_head - history_count + history_idx + HISTORY_SIZE) % HISTORY_SIZE;
                                strncpy(cmd_buffer, history[actual], MAX_COMMAND_LENGTH - 1);
                                cmd_buffer[MAX_COMMAND_LENGTH - 1] = '\0';
                                cmd_len = strlen(cmd_buffer);
                                cursor_pos = cmd_len;
                            } else {
                                history_idx = -1;
                                cmd_buffer[0] = '\0';
                                cmd_len = 0;
                                cursor_pos = 0;
                            }
                            redraw_line(prompt, cmd_buffer, cmd_len, cursor_pos);
                        }
                        break;
                }
                continue;
            }

            char c = (char)(key & 0xFF);

            if (c == 0x03) { // Ctrl+C
                printk("^C\n%s", prompt);
                cmd_len = 0; cursor_pos = 0; cmd_buffer[0] = '\0';
                continue;
            }
            if (c == 0x0C) { // Ctrl+L
                tty_clear();
                printk("%s%s", prompt, cmd_buffer);
                continue;
            }
            if (c == '\b') {
                if (cursor_pos > 0) {
                    for (size_t i = cursor_pos - 1; i < cmd_len - 1; i++) cmd_buffer[i] = cmd_buffer[i + 1];
                    cmd_len--;
                    cursor_pos--;
                    cmd_buffer[cmd_len] = '\0';
                    redraw_line(prompt, cmd_buffer, cmd_len, cursor_pos);
                }
            } else if (c == '\t') {
                if (cmd_len > 0) {
                    int matches = 0;
                    const char* last = NULL;
                    for (int i = 0; shell_builtins[i] != NULL; i++) {
                        if (strncmp(shell_builtins[i], cmd_buffer, cmd_len) == 0) {
                            matches++;
                            last = shell_builtins[i];
                        }
                    }
                    if (matches == 1 && last) {
                        strncpy(cmd_buffer, last, MAX_COMMAND_LENGTH - 1);
                        cmd_len = strlen(cmd_buffer);
                        cursor_pos = cmd_len;
                        redraw_line(prompt, cmd_buffer, cmd_len, cursor_pos);
                    } else if (matches > 1) {
                        printk("\n");
                        for (int i = 0; shell_builtins[i] != NULL; i++) {
                            if (strncmp(shell_builtins[i], cmd_buffer, cmd_len) == 0) {
                                printk("  %s", shell_builtins[i]);
                            }
                        }
                        printk("\n");
                        redraw_line(prompt, cmd_buffer, cmd_len, cursor_pos);
                    }
                }
            } else if (c == '\n' || c == '\r') {
                printk("\n");
                cmd_buffer[cmd_len] = '\0';
                break;
            } else if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
                if (cmd_len < MAX_COMMAND_LENGTH - 1) {
                    for (size_t i = cmd_len; i > cursor_pos; i--) cmd_buffer[i] = cmd_buffer[i - 1];
                    cmd_buffer[cursor_pos] = c;
                    cmd_len++;
                    cursor_pos++;
                    cmd_buffer[cmd_len] = '\0';
                    redraw_line(prompt, cmd_buffer, cmd_len, cursor_pos);
                }
            }
        }

        if (cmd_len > 0) {
            history_add(cmd_buffer);
            shell_process(cmd_buffer);
        }
    }
}
