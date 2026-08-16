#include <arch/x86_64/pit.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/io.h>
#include <kernel/sched.h>

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_BASE_FREQUENCY 1193182

static volatile uint64_t pit_ticks = 0;
static uint32_t pit_freq = 100;

static void pit_handler(registers_t* regs) {
    (void)regs;
    pit_ticks++;

    // Notify preemptive scheduler
    sched_tick();
}

void pit_init(uint32_t frequency) {
    pit_freq = frequency;
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

    // Channel 0, lobyte/hibyte, Rate generator (Mode 2)
    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    // Register IRQ0 handler
    isr_register_handler(32, pit_handler);
    pic_clear_mask(0);
}

uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

uint64_t pit_get_uptime_ms(void) {
    return (pit_ticks * 1000) / pit_freq;
}

void pit_sleep(uint32_t milliseconds) {
    uint64_t target = pit_ticks + (milliseconds * pit_freq) / 1000;
    while (pit_ticks < target) {
        hlt();
    }
}
