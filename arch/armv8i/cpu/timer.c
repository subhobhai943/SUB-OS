#include <arch/armv8i/timer.h>
#include <arch/armv8i/gic.h>
#include <kernel/timer.h>
#include <kernel/printk.h>

static volatile uint64_t armv8i_timer_ticks = 0;
static uint32_t timer_freq = 0;
static uint32_t tick_interval = 0;

uint64_t armv8i_timer_get_ticks(void) {
    return armv8i_timer_ticks;
}

uint64_t armv8i_timer_get_frequency(void) {
    return timer_freq;
}

void armv8i_timer_handle_irq(void) {
    armv8i_timer_ticks++;

    // Reset comparator for next tick
    __asm__ volatile("mcr p15, 0, %0, c14, c3, 0" :: "r"(tick_interval));

    timer_tick();
}

void armv8i_timer_init(uint32_t freq_hz) {
    // 1. Read base counter frequency (CNTFRQ)
    __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(timer_freq));
    if (timer_freq == 0) {
        timer_freq = 62500000; // 62.5 MHz default in QEMU virt
    }

    tick_interval = timer_freq / (freq_hz ? freq_hz : 100);

    // 2. Set interval in CNTV_TVAL
    __asm__ volatile("mcr p15, 0, %0, c14, c3, 0" :: "r"(tick_interval));

    // 3. Enable Virtual Timer (ENABLE=1, IMASK=0 in CNTV_CTL)
    uint32_t ctl = 1;
    __asm__ volatile("mcr p15, 0, %0, c14, c3, 1" :: "r"(ctl));

    printk(KERN_INFO "Timer: ARM Generic Arch Timer active (%u Hz, Base Clock: %u MHz)\n",
           freq_hz, timer_freq / 1000000);
}
