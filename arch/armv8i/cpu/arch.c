#include <arch/arch.h>
#include <arch/armv8i/cpu.h>
#include <arch/armv8i/gic.h>
#include <arch/armv8i/timer.h>
#include <arch/armv8i/uart.h>
#include <arch/armv8i/mmu.h>
#include <arch/x86_64/cpuid.h>
#include <kernel/printk.h>
#include <lib/string.h>

static cpu_info_t armv8i_compat_cpu;

void arch_early_init(void) {
    uart_pl011_init();
    armv8i_mmu_init();
}

void arch_init(void) {
    armv8i_cpu_init();
    gic_init();
    armv8i_timer_init(100);

    // Populate compatibility CPU info structure for procfs / uname
    memset(&armv8i_compat_cpu, 0, sizeof(armv8i_compat_cpu));
    const armv8i_cpu_info_t* info = armv8i_get_cpu_info();
    strncpy(armv8i_compat_cpu.vendor, "ARM Ltd", sizeof(armv8i_compat_cpu.vendor) - 1);
    strncpy(armv8i_compat_cpu.model, info->model_name, sizeof(armv8i_compat_cpu.model) - 1);
    armv8i_compat_cpu.has_fpu = true;

    arch_enable_interrupts();
}

const char* arch_get_name(void) {
    return "armv8i (AArch32)";
}

uint32_t arch_get_cpu_id(void) {
    return armv8i_get_mpidr() & 0xFF;
}

// Timer Compatibility Layer
uint64_t pit_get_ticks(void) {
    return armv8i_timer_get_ticks();
}

void pit_sleep(uint32_t ms) {
    uint64_t count = (ms > 10) ? (ms / 10) : 1;
    uint64_t target = armv8i_timer_get_ticks() + count;
    while (armv8i_timer_get_ticks() < target) {
        __asm__ volatile("wfi");
    }
}

// Stubs for x86-specific driver compatibility
void isr_register_handler(uint8_t isr, void* handler) { (void)isr; (void)handler; }
void pic_clear_mask(uint8_t irq) { (void)irq; }

const cpu_info_t* cpuid_get_info(void) {
    return &armv8i_compat_cpu;
}
