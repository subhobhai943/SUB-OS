#include <drivers/serial.h>
#include <arch/x86_64/io.h>

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Disable all interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1_PORT + 1, 0x00); //                  (hi byte)
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static bool serial_is_transmit_empty(void) {
    return (inb(COM1_PORT + 5) & 0x20) != 0;
}

void serial_write_char(char c) {
    while (!serial_is_transmit_empty());
    outb(COM1_PORT, c);
}

void serial_write_string(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*str++);
    }
}

bool serial_received(void) {
    return (inb(COM1_PORT + 5) & 1) != 0;
}

char serial_read_char(void) {
    while (!serial_received());
    return inb(COM1_PORT);
}
