#include <arch/arch.h>
#include <arch/aarch64/cpu.h>
#include <arch/aarch64/gic.h>
#include <arch/aarch64/timer.h>
#include <arch/aarch64/uart.h>
#include <arch/aarch64/mmu.h>
#include <arch/x86_64/cpuid.h>
#include <kernel/printk.h>
#include <lib/string.h>

static cpu_info_t aarch64_compat_cpu;

void arch_early_init(void) {
    uart_pl011_init();
}

void arch_init(void) {
    aarch64_cpu_init();
    gic_init();
    aarch64_timer_init(100);
    aarch64_mmu_init();

    // Populate compatibility CPU info structure for procfs
    memset(&aarch64_compat_cpu, 0, sizeof(aarch64_compat_cpu));
    const aarch64_cpu_info_t* info = aarch64_get_cpu_info();
    strncpy(aarch64_compat_cpu.vendor, "ARM Ltd", sizeof(aarch64_compat_cpu.vendor) - 1);
    strncpy(aarch64_compat_cpu.model, info->model_name, sizeof(aarch64_compat_cpu.model) - 1);
    aarch64_compat_cpu.has_fpu = true;

    arch_enable_interrupts();
}

const char* arch_get_name(void) {
    return "aarch64 (ARMv8-A)";
}

uint32_t arch_get_cpu_id(void) {
    return (uint32_t)(aarch64_get_mpidr() & 0xFF);
}

// Timer Compatibility Layer for Portable Drivers
uint64_t pit_get_ticks(void) {
    return aarch64_timer_get_ticks();
}

void pit_sleep(uint32_t ms) {
    uint64_t count = (ms > 10) ? (ms / 10) : 1;
    uint64_t target = aarch64_timer_get_ticks() + count;
    while (aarch64_timer_get_ticks() < target) {
        __asm__ volatile("wfi");
    }
}

// Interrupt & CPUID Compatibility Stubs
void isr_register_handler(uint8_t isr, void* handler) {
    (void)isr; (void)handler;
}

void pic_clear_mask(uint8_t irq) {
    (void)irq;
}

const cpu_info_t* cpuid_get_info(void) {
    return &aarch64_compat_cpu;
}
