#include <security/auth.h>
#include <crypto/crypto.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <drivers/keyboard.h>

static user_account_t user_table[MAX_USERS];
static size_t user_count = 0;
static uint32_t current_uid = 0;

static void hash_password(const char* password, char* out_hex) {
    uint8_t digest[32];
    sha256((const uint8_t*)password, strlen(password), digest);
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out_hex[i * 2]     = hex_chars[(digest[i] >> 4) & 0x0F];
        out_hex[i * 2 + 1] = hex_chars[digest[i] & 0x0F];
    }
    out_hex[64] = '\0';
}

void auth_init(void) {
    memset(user_table, 0, sizeof(user_table));
    user_count = 0;
    current_uid = 0;

    // Seed default users. SUB/SUB is the primary superuser account -- the way
    // Kali ships with kali/kali -- so the username and password both default to
    // "SUB". It is seeded first, so uid 0 resolves to it as the logged-in user.
    auth_add_user("SUB", "SUB", 0, 0, "/root", "/bin/sh");
    auth_add_user("root", "root", 0, 0, "/root", "/bin/sh"); // kept for compatibility
    auth_add_user("admin", "admin123", 1000, 1000, "/home/admin", "/bin/sh");
    auth_add_user("guest", "guest", 1001, 1001, "/home/guest", "/bin/sh");

    // Log in as SUB by default.
    auth_set_current_user("SUB");

    printk(KERN_INFO "AUTH: Pluggable Authentication & SHA-256 Shadow Password engine active\n");
}

int auth_add_user(const char* username, const char* password, uint32_t uid, uint32_t gid, const char* home, const char* shell) {
    if (!username) return -1;

    for (size_t i = 0; i < MAX_USERS; i++) {
        if (!user_table[i].in_use) {
            user_account_t* u = &user_table[i];
            u->in_use = true;
            u->uid = uid;
            u->gid = gid;
            strncpy(u->username, username, sizeof(u->username) - 1);
            strncpy(u->home_dir, home ? home : "/home", sizeof(u->home_dir) - 1);
            strncpy(u->shell, shell ? shell : "/bin/sh", sizeof(u->shell) - 1);

            if (password) {
                hash_password(password, u->password_hash);
            } else {
                u->password_hash[0] = '\0';
            }

            if (i >= user_count) user_count = i + 1;
            return 0;
        }
    }
    return -1;
}

bool auth_verify_password(const char* username, const char* password) {
    if (!username || !password) return false;
    const user_account_t* u = auth_get_user_by_name(username);
    if (!u) return false;

    char test_hash[65];
    hash_password(password, test_hash);
    return (strcmp(u->password_hash, test_hash) == 0);
}

int auth_change_password(const char* username, const char* new_password) {
    if (!username || !new_password) return -1;
    for (size_t i = 0; i < MAX_USERS; i++) {
        if (user_table[i].in_use && strcmp(user_table[i].username, username) == 0) {
            hash_password(new_password, user_table[i].password_hash);
            return 0;
        }
    }
    return -1;
}

const user_account_t* auth_get_user_by_name(const char* username) {
    if (!username) return NULL;
    for (size_t i = 0; i < MAX_USERS; i++) {
        if (user_table[i].in_use && strcmp(user_table[i].username, username) == 0) {
            return &user_table[i];
        }
    }
    return NULL;
}

const user_account_t* auth_get_user_by_uid(uint32_t uid) {
    for (size_t i = 0; i < MAX_USERS; i++) {
        if (user_table[i].in_use && user_table[i].uid == uid) {
            return &user_table[i];
        }
    }
    return NULL;
}

const user_account_t* auth_get_current_user(void) {
    const user_account_t* u = auth_get_user_by_uid(current_uid);
    return u ? u : &user_table[0];
}

int auth_set_current_user(const char* username) {
    const user_account_t* u = auth_get_user_by_name(username);
    if (!u) return -1;
    current_uid = u->uid;
    return 0;
}

size_t auth_get_user_count(void) {
    return user_count;
}

const user_account_t* auth_get_user_by_index(size_t index) {
    if (index >= MAX_USERS || !user_table[index].in_use) return NULL;
    return &user_table[index];
}

/* Read a line from the PS/2 keyboard into buf. When echo is false the typed
 * characters are masked with '*' (password entry). Blocks until Enter. */
static void login_read_line(char* buf, size_t max, bool echo) {
    size_t len = 0;
    while (true) {
        uint16_t key = keyboard_get_key();
        if (key & KEY_SPECIAL_FLAG) {
            continue; // ignore arrows / function keys at the login prompt
        }
        char c = (char)(key & 0xFF);

        if (c == '\n' || c == '\r') {
            printk("\n");
            break;
        }
        if (c == '\b' || (unsigned char)c == 0x7F || c == 0x08) {
            if (len > 0) {
                len--;
                printk("\b \b"); // erase the last glyph on screen
            }
            continue;
        }
        if ((unsigned char)c >= 32 && (unsigned char)c <= 126 && len < max - 1) {
            buf[len++] = c;
            if (echo) {
                printk("%c", c);
            } else {
                printk("*");
            }
        }
    }
    buf[len] = '\0';
}

void console_login(void) {
    char username[32];
    char password[64];

    printk("\n" ANSI_BRIGHT_CYAN
           "=================================================================\n"
           "   SUB-OS Secure Login -- authenticate to start your session\n"
           "=================================================================\n"
           ANSI_RESET);
    printk(ANSI_YELLOW "   Default credentials: username " ANSI_BRIGHT_GREEN "SUB"
           ANSI_YELLOW ", password " ANSI_BRIGHT_GREEN "SUB" ANSI_RESET "\n\n");

    while (true) {
        printk(ANSI_BRIGHT_GREEN "sub-os " ANSI_RESET "login: ");
        login_read_line(username, sizeof(username), true);
        if (username[0] == '\0') {
            continue; // empty username, re-prompt
        }

        printk("Password: ");
        login_read_line(password, sizeof(password), false);

        bool ok = auth_verify_password(username, password);

        // Scrub the password from the stack regardless of the outcome.
        memset(password, 0, sizeof(password));

        if (ok) {
            auth_set_current_user(username);
            printk(ANSI_BRIGHT_GREEN
                   "\nLogin successful. Welcome to SUB-OS, %s!\n\n" ANSI_RESET,
                   username);
            return;
        }

        printk(ANSI_BRIGHT_RED "\nLogin incorrect. Please try again.\n\n" ANSI_RESET);
    }
}
