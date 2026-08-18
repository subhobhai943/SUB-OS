#include <arch/aarch64/timer.h>
#include <arch/aarch64/gic.h>
#include <kernel/timer.h>
#include <kernel/printk.h>

static uint64_t timer_ticks = 0;
static uint64_t timer_freq = 62500000; // Standard 62.5MHz QEMU virt frequency
static uint32_t tick_interval = 0;

void aarch64_timer_handle_irq(void) {
    timer_ticks++;

    // Reset next comparator interval
    __asm__ volatile("msr cntv_tval_el0, %0" :: "r"((uint64_t)tick_interval));

    // Call generic kernel timer tick
    timer_tick();
}

static void gic_timer_wrapper(uint32_t irq) {
    (void)irq;
    aarch64_timer_handle_irq();
}

void aarch64_timer_init(uint32_t freq_hz) {
    if (freq_hz == 0) freq_hz = 100;

    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(timer_freq));
    tick_interval = (uint32_t)(timer_freq / freq_hz);

    // Register with GIC (PPI 27 = Virtual Timer IRQ)
    gic_register_handler(ARCH_TIMER_VIRT_IRQ, gic_timer_wrapper);

    // Set countdown and enable virtual timer
    __asm__ volatile("msr cntv_tval_el0, %0" :: "r"((uint64_t)tick_interval));
    uint64_t ctl = 1; // Enable=1, IMASK=0
    __asm__ volatile("msr cntv_ctl_el0, %0" :: "r"(ctl));

    printk(KERN_INFO "Timer: ARM Generic Arch Timer active (%u Hz, Base Clock: %llu MHz)\n",
           freq_hz, timer_freq / 1000000);
}

uint64_t aarch64_timer_get_ticks(void) {
    return timer_ticks;
}

uint64_t aarch64_timer_get_freq(void) {
    return timer_freq;
}
