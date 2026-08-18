#include <arch/armv8i/arch.h>
#include <arch/armv8i/gic.h>
#include <arch/armv8i/timer.h>
#include <arch/armv8i/uart.h>
#include <kernel/printk.h>

typedef struct {
    uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;
    uint32_t lr;
    uint32_t spsr;
} armv8i_registers_t;

static void hex_to_str(uint32_t val, char* out) {
    const char hex_chars[] = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        out[2 + (7 - i)] = hex_chars[(val >> (i * 4)) & 0xF];
    }
    out[10] = '\0';
}

void armv8i_undef_handler(armv8i_registers_t* regs) {
    char buf[16];
    hex_to_str(regs->lr, buf);
    uart_pl011_puts("\n[KERNEL PANIC] ARMv8i Undefined Instruction at PC=");
    uart_pl011_puts(buf);
    uart_pl011_puts("\n");
    while (1) { __asm__ volatile("wfi"); }
}

void armv8i_svc_handler(armv8i_registers_t* regs) {
    (void)regs;
}

void armv8i_pabt_handler(armv8i_registers_t* regs) {
    char buf[16];
    hex_to_str(regs->lr, buf);
    uart_pl011_puts("\n[KERNEL PANIC] ARMv8i Prefetch Abort at PC=");
    uart_pl011_puts(buf);
    uart_pl011_puts("\n");
    while (1) { __asm__ volatile("wfi"); }
}

void armv8i_dabt_handler(armv8i_registers_t* regs) {
    char buf[16];
    hex_to_str(regs->lr, buf);
    uart_pl011_puts("\n[KERNEL PANIC] ARMv8i Data Abort at PC=");
    uart_pl011_puts(buf);
    uart_pl011_puts("\n");
    while (1) { __asm__ volatile("wfi"); }
}

void armv8i_irq_handler(armv8i_registers_t* regs) {
    (void)regs;
    uint32_t iar = *(volatile uint32_t*)GICC_IAR;
    uint32_t irq = iar & 0x3FF;

    if (irq < 1020) {
        if (irq == ARMV8I_TIMER_VIRT_IRQ) {
            armv8i_timer_handle_irq();
        }
        *(volatile uint32_t*)GICC_EOIR = iar;
    }
}

void armv8i_fiq_handler(armv8i_registers_t* regs) {
    (void)regs;
}
