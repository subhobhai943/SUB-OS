#ifndef _ARCH_AARCH64_UART_H
#define _ARCH_AARCH64_UART_H

#include <stdint.h>
#include <stddef.h>

#define PL011_UART0_BASE 0x09000000

// PL011 Registers
#define UARTDR   0x000
#define UARTRSR  0x004
#define UARTFR   0x018
#define UARTILPR 0x020
#define UARTIBRD 0x024
#define UARTFBRD 0x028
#define UARTLCR  0x02C
#define UARTCR   0x030
#define UARTIFLS 0x034
#define UARTIMSC 0x038
#define UARTRIS  0x03C
#define UARTMIS  0x040
#define UARTICR  0x044

// Flag register bits
#define UARTFR_TXFF (1 << 5)
#define UARTFR_RXFE (1 << 4)

void uart_pl011_init(void);
void uart_pl011_putc(char c);
void uart_pl011_puts(const char* str);
char uart_pl011_getc(void);
int  uart_pl011_has_data(void);

#endif // _ARCH_AARCH64_UART_H
