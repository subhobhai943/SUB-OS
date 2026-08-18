#ifndef _ARCH_ARMV8I_UART_H
#define _ARCH_ARMV8I_UART_H

#include <stdint.h>

#define PL011_UART0_BASE 0x09000000

#define UARTDR   0x000
#define UARTRSR  0x004
#define UARTFR   0x018
#define UARTCR   0x030
#define UARTIMSC 0x038
#define UARTICR  0x044

#define UARTFR_TXFF (1 << 5)
#define UARTFR_RXFE (1 << 4)

void uart_pl011_init(void);
void uart_pl011_putc(char c);
void uart_pl011_puts(const char* str);
char uart_pl011_getc(void);
int  uart_pl011_has_data(void);

#endif // _ARCH_ARMV8I_UART_H
