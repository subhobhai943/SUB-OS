#include <arch/arch.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpuid.h>
#include <kernel/timer.h>

static void enable_fpu_sse(void) {
    uint64_t cr0, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Clear EM (x87 emulation)
    cr0 |= (1ULL << 1);  // Set MP (Monitor co-processor)
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));

    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10); // Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
}

void arch_early_init(void) {
    // x86_64 serial and tty initialized in main
}

void arch_init(void) {
    enable_fpu_sse();
    gdt_init();
    idt_init();
    isr_init();
    pic_init();
    pit_init(100);
    timer_wheel_init();
    arch_enable_interrupts();
}

const char* arch_get_name(void) {
    return "x86_64 (AMD64)";
}

uint32_t arch_get_cpu_id(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (ebx >> 24) & 0xFF;
}
