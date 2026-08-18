#include <arch/aarch64/arch.h>
#include <arch/aarch64/gic.h>
#include <kernel/printk.h>

void aarch64_sync_handler(aarch64_context_t* ctx) {
    uint64_t esr, elr, far;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));

    uint32_t ec = (esr >> 26) & 0x3F;
    printk(ANSI_BRIGHT_RED "\n[KERNEL PANIC] AArch64 Synchronous Exception!\n" ANSI_RESET);
    printk("ESR_EL1: 0x%016llx (EC: 0x%02x, ISS: 0x%08x)\n", esr, ec, (uint32_t)(esr & 0x01FFFFFF));
    printk("ELR_EL1 (PC): 0x%016llx  FAR_EL1: 0x%016llx\n", elr, far);
    printk("Registers:\n");
    for (int i = 0; i < 30; i += 2) {
        printk("  X%02d: 0x%016llx   X%02d: 0x%016llx\n", i, ctx->r[i], i + 1, ctx->r[i + 1]);
    }
    printk("  X30 (LR): 0x%016llx   SP: 0x%016llx\n", ctx->r[30], ctx->sp);

    while (1) {
        __asm__ volatile("wfe");
    }
}

void aarch64_irq_handler(aarch64_context_t* ctx) {
    (void)ctx;
    gic_handle_irq();
}

void aarch64_fiq_handler(aarch64_context_t* ctx) {
    (void)ctx;
    printk(KERN_WARNING "AArch64: Unexpected FIQ caught\n");
}

void aarch64_serror_handler(aarch64_context_t* ctx) {
    (void)ctx;
    printk(ANSI_BRIGHT_RED "[KERNEL PANIC] AArch64 SError Interrupt caught!\n" ANSI_RESET);
    while (1) {
        __asm__ volatile("wfe");
    }
}
