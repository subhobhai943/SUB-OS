#ifndef _ARCH_ARCH_H
#define _ARCH_ARCH_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__x86_64__)
#define ARCH_NAME "x86_64"
#define ARCH_IS_64BIT 1
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpuid.h>
#include <arch/x86_64/paging.h>
#include <arch/x86_64/io.h>

static inline void arch_enable_interrupts(void) {
    __asm__ volatile("sti");
}

static inline void arch_disable_interrupts(void) {
    __asm__ volatile("cli");
}

/* Save the current interrupt-enable state and disable interrupts; restore puts
 * it back exactly. Used to make a critical section atomic against IRQ handlers
 * (e.g. the preemptive scheduler) without unconditionally re-enabling. */
static inline unsigned long arch_irq_save(void) {
    unsigned long flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void arch_irq_restore(unsigned long flags) {
    __asm__ volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
}

static inline void arch_halt(void) {
    __asm__ volatile("hlt");
}

static inline void arch_wait_for_interrupt(void) {
    __asm__ volatile("sti; hlt");
}

static inline uint64_t arch_get_ticks(void) {
    return pit_get_ticks();
}

static inline void arch_sleep(uint32_t ms) {
    pit_sleep(ms);
}

#elif defined(__aarch64__) || defined(__arm64__)
#define ARCH_NAME "aarch64"
#define ARCH_IS_64BIT 1
#include <arch/aarch64/arch.h>
#include <arch/aarch64/cpu.h>
#include <arch/aarch64/gic.h>
#include <arch/aarch64/timer.h>
#include <arch/aarch64/mmu.h>
#include <arch/aarch64/uart.h>
#include <arch/aarch64/io.h>

static inline void arch_enable_interrupts(void) {
    __asm__ volatile("msr daifclr, #2" ::: "memory");
}

static inline void arch_disable_interrupts(void) {
    __asm__ volatile("msr daifset, #2" ::: "memory");
}

static inline unsigned long arch_irq_save(void) {
    unsigned long flags;
    __asm__ volatile("mrs %0, daif; msr daifset, #2" : "=r"(flags) :: "memory");
    return flags;
}

static inline void arch_irq_restore(unsigned long flags) {
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
}

static inline void arch_halt(void) {
    __asm__ volatile("wfi");
}

static inline void arch_wait_for_interrupt(void) {
    __asm__ volatile("msr daifclr, #2; wfi" ::: "memory");
}

static inline uint64_t arch_get_ticks(void) {
    return aarch64_timer_get_ticks();
}

static inline void arch_sleep(uint32_t ms) {
    uint64_t count = (ms > 10) ? (ms / 10) : 1;
    uint64_t target = aarch64_timer_get_ticks() + count;
    while (aarch64_timer_get_ticks() < target) {
        __asm__ volatile("wfi");
    }
}

uint64_t pit_get_ticks(void);
void     pit_sleep(uint32_t ms);

#elif defined(__arm__) || defined(__armv8i__)
#define ARCH_NAME "armv8i"
#define ARCH_IS_64BIT 0
#include <arch/armv8i/arch.h>
#include <arch/armv8i/cpu.h>
#include <arch/armv8i/gic.h>
#include <arch/armv8i/timer.h>
#include <arch/armv8i/mmu.h>
#include <arch/armv8i/uart.h>
#include <arch/armv8i/io.h>

static inline void arch_enable_interrupts(void) {
    __asm__ volatile("cpsie i" ::: "memory");
}

static inline void arch_disable_interrupts(void) {
    __asm__ volatile("cpsid i" ::: "memory");
}

static inline unsigned long arch_irq_save(void) {
    unsigned long flags;
    __asm__ volatile("mrs %0, cpsr; cpsid i" : "=r"(flags) :: "memory");
    return flags;
}

static inline void arch_irq_restore(unsigned long flags) {
    __asm__ volatile("msr cpsr_c, %0" :: "r"(flags) : "memory", "cc");
}

static inline void arch_halt(void) {
    __asm__ volatile("wfi");
}

static inline void arch_wait_for_interrupt(void) {
    __asm__ volatile("cpsie i; wfi" ::: "memory");
}

static inline uint64_t arch_get_ticks(void) {
    return armv8i_timer_get_ticks();
}

static inline void arch_sleep(uint32_t ms) {
    uint64_t count = (ms > 10) ? (ms / 10) : 1;
    uint64_t target = armv8i_timer_get_ticks() + count;
    while (armv8i_timer_get_ticks() < target) {
        __asm__ volatile("wfi");
    }
}

uint64_t pit_get_ticks(void);
void     pit_sleep(uint32_t ms);

#else
#error "Unsupported Target CPU Architecture!"
#endif

// Generic Architecture API
void        arch_early_init(void);
void        arch_init(void);
const char* arch_get_name(void);
uint32_t    arch_get_cpu_id(void);

#endif // _ARCH_ARCH_H
