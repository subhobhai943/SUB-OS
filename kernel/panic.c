#include <kernel/panic.h>
#include <kernel/printk.h>
#include <drivers/tty.h>
#include <arch/arch.h>

void panic_with_regs(const char* message, const registers_t* regs) {
    arch_disable_interrupts();

    printk("\n" ANSI_BRIGHT_RED "================================================================================" ANSI_RESET "\n");
    printk(ANSI_BRIGHT_RED "                     *** KERNEL PANIC - SYSTEM HALTED ***                      " ANSI_RESET "\n");
    printk(ANSI_BRIGHT_RED "================================================================================" ANSI_RESET "\n\n");
    printk(ANSI_BRIGHT_WHITE "Reason: " ANSI_YELLOW "%s" ANSI_RESET "\n\n", message ? message : "Unknown Fault");

    if (regs) {
        printk(ANSI_BRIGHT_CYAN "CPU Register State:" ANSI_RESET "\n");
        printk("  RIP: %016llx  CS:  %04llx  RFLAGS: %016llx\n", regs->rip, regs->cs, regs->rflags);
        printk("  RSP: %016llx  SS:  %04llx  RBP:    %016llx\n", regs->rsp, regs->ss, regs->rbp);
        printk("  RAX: %016llx  RBX: %016llx  RCX:    %016llx  RDX: %016llx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        printk("  RSI: %016llx  RDI: %016llx  R8:     %016llx  R9:  %016llx\n", regs->rsi, regs->rdi, regs->r8, regs->r9);
        printk("  R10: %016llx  R11: %016llx  R12:    %016llx  R13: %016llx\n", regs->r10, regs->r11, regs->r12, regs->r13);
        printk("  R14: %016llx  R15: %016llx  ERR:    %016llx  INT: %llu\n", regs->r14, regs->r15, regs->error_code, regs->int_no);
    }

    printk("\n" ANSI_BRIGHT_YELLOW "Please power off or reset your system." ANSI_RESET "\n");

    while (1) {
        arch_halt();
    }
}

void panic(const char* message) {
    panic_with_regs(message, NULL);
}
