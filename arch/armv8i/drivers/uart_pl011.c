#include <arch/armv8i/uart.h>
#include <arch/armv8i/io.h>

void uart_pl011_init(void) {
    // Enable UART, TX, and RX (bits 0, 8, 9)
    mmio_write32(PL011_UART0_BASE + UARTCR, (1 << 0) | (1 << 8) | (1 << 9));
}

void uart_pl011_putc(char c) {
    if (c == '\n') {
        while (mmio_read32(PL011_UART0_BASE + UARTFR) & UARTFR_TXFF) {
            __asm__ volatile("nop");
        }
        mmio_write32(PL011_UART0_BASE + UARTDR, '\r');
    }
    while (mmio_read32(PL011_UART0_BASE + UARTFR) & UARTFR_TXFF) {
        __asm__ volatile("nop");
    }
    mmio_write32(PL011_UART0_BASE + UARTDR, (uint32_t)(uint8_t)c);
}

void uart_pl011_puts(const char* str) {
    if (!str) return;
    while (*str) {
        uart_pl011_putc(*str++);
    }
}

char uart_pl011_getc(void) {
    while (mmio_read32(PL011_UART0_BASE + UARTFR) & UARTFR_RXFE) {
        __asm__ volatile("wfe");
    }
    return (char)(mmio_read32(PL011_UART0_BASE + UARTDR) & 0xFF);
}

int uart_pl011_has_data(void) {
    return !(mmio_read32(PL011_UART0_BASE + UARTFR) & UARTFR_RXFE);
}
