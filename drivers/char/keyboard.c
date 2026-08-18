#include <drivers/keyboard.h>
#include <drivers/tty.h>
#include <arch/arch.h>

#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_BUFFER_SIZE  128

static volatile uint16_t key_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int buffer_head = 0;
static volatile int buffer_tail = 0;

static bool shift_pressed = false;
static bool ctrl_pressed  = false;
static bool alt_pressed   = false;
static bool capslock      = false;
static bool extended_code = false;

static const char scancode_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char scancode_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void buffer_enqueue(uint16_t key) {
    int next = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != buffer_tail) {
        key_buffer[buffer_head] = key;
        buffer_head = next;
    }
}

#if defined(__x86_64__)
static void keyboard_interrupt_handler(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode == 0xE0) {
        extended_code = true;
        return;
    }

    if (scancode & 0x80) { // Key Release
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) shift_pressed = false;
        else if (released == 0x1D) ctrl_pressed = false;
        else if (released == 0x38) alt_pressed = false;
        extended_code = false;
        return;
    }

    // Key Press
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = true; return; }
    if (scancode == 0x1D) { ctrl_pressed = true; return; }
    if (scancode == 0x38) { alt_pressed = true; return; }
    if (scancode == 0x3A) { capslock = !capslock; return; }

    // Alt + F1..F4 -> Switch Virtual Consoles
    if (alt_pressed) {
        if (scancode >= 0x3B && scancode <= 0x3E) { // F1 - F4
            tty_switch(scancode - 0x3B);
            return;
        }
    }

    if (extended_code) {
        extended_code = false;
        switch (scancode) {
            case 0x48: buffer_enqueue(KEY_UP); return;
            case 0x50: buffer_enqueue(KEY_DOWN); return;
            case 0x4B: buffer_enqueue(KEY_LEFT); return;
            case 0x4D: buffer_enqueue(KEY_RIGHT); return;
            case 0x47: buffer_enqueue(KEY_HOME); return;
            case 0x4F: buffer_enqueue(KEY_END); return;
            case 0x49: buffer_enqueue(KEY_PAGE_UP); return;
            case 0x51: buffer_enqueue(KEY_PAGE_DOWN); return;
            case 0x52: buffer_enqueue(KEY_INSERT); return;
            case 0x53: buffer_enqueue(KEY_DELETE); return;
        }
    }

    // Function Keys F1-F12
    if (scancode >= 0x3B && scancode <= 0x44) {
        buffer_enqueue(KEY_F1 + (scancode - 0x3B));
        return;
    }

    if (ctrl_pressed) {
        char base = scancode_ascii[scancode];
        if (base >= 'a' && base <= 'z') {
            buffer_enqueue((uint16_t)(base - 'a' + 1));
            return;
        }
    }

    bool use_shift = shift_pressed ^ capslock;
    char c = use_shift ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
    if (c) {
        buffer_enqueue((uint16_t)c);
    }
}
#endif

void keyboard_init(void) {
    buffer_head = 0;
    buffer_tail = 0;

#if defined(__x86_64__)
    isr_register_handler(33, keyboard_interrupt_handler); // IRQ1
    pic_clear_mask(1);
#endif
}

#if defined(__aarch64__)
#include <arch/aarch64/uart.h>
#endif

bool keyboard_has_key(void) {
#if defined(__aarch64__)
    if (uart_pl011_has_data()) return true;
#endif
    return buffer_head != buffer_tail;
}

uint16_t keyboard_get_key(void) {
    while (!keyboard_has_key()) {
        arch_halt();
    }
#if defined(__aarch64__)
    if (uart_pl011_has_data()) {
        char c = uart_pl011_getc();
        if (c == '\r') c = '\n';
        return (uint16_t)(uint8_t)c;
    }
#endif
    uint16_t key = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return key;
}

char keyboard_get_char(void) {
    uint16_t key = keyboard_get_key();
    if (key & KEY_SPECIAL_FLAG) return 0;
    return (char)(key & 0xFF);
}
