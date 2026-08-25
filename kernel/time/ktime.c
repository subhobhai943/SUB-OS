// Unified monotonic high-resolution clock for SUB-OS.
//
// When the HPET is available it provides true nanosecond timing; otherwise the
// clock degrades gracefully to the 100 Hz kernel timer tick (10 ms resolution).
// Either way the epoch is the moment ktime_init() ran.

#include <kernel/ktime.h>
#include <drivers/hpet.h>
#include <arch/arch.h>
#include <kernel/printk.h>

#define TICK_HZ        100ULL
#define NS_PER_TICK    (1000000000ULL / TICK_HZ)   // 10,000,000 ns at 100 Hz

static bool     g_use_hpet = false;
static uint64_t g_base_tick = 0;

void ktime_init(void) {
    g_use_hpet = hpet_available();
    g_base_tick = pit_get_ticks();
    printk(ANSI_BRIGHT_GREEN "KTIME: " ANSI_RESET
           "Monotonic clock online (source: %s%s)\n",
           g_use_hpet ? "HPET" : "PIT 100 Hz",
           g_use_hpet ? ", nanosecond resolution" : "");
}

uint64_t ktime_ns(void) {
    if (g_use_hpet) {
        return hpet_get_ns();
    }
    uint64_t ticks = pit_get_ticks() - g_base_tick;
    return ticks * NS_PER_TICK;
}

uint64_t ktime_us(void) { return ktime_ns() / 1000ULL; }
uint64_t ktime_ms(void) { return ktime_ns() / 1000000ULL; }

const char* ktime_source(void) { return g_use_hpet ? "HPET" : "PIT"; }
bool        ktime_is_highres(void) { return g_use_hpet; }
