#include <arch/arch.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpuid.h>
#include <kernel/timer.h>

void arch_early_init(void) {
    // x86_64 serial and tty initialized in main
}

void arch_init(void) {
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
