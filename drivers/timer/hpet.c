// High Precision Event Timer (HPET) driver for SUB-OS.
//
// Reads the HPET block memory-mapped at the standard base 0xFED00000 (as
// exposed by QEMU). The general-capabilities register carries the counter
// period in femtoseconds, from which the tick frequency and a nanosecond
// timestamp are derived. The kernel identity-maps low physical memory, so the
// MMIO block is reachable directly through its physical address.

#include <drivers/hpet.h>
#include <kernel/printk.h>

#if defined(__x86_64__)

#define HPET_BASE_PHYS      0xFED00000ULL
#define HPET_REG_CAP_ID     0x000   // General Capabilities and ID (64-bit)
#define HPET_REG_CONFIG     0x010   // General Configuration (64-bit)
#define HPET_REG_COUNTER    0x0F0   // Main Counter Value (64-bit)

#define HPET_CFG_ENABLE     0x1ULL  // General Config bit 0: overall enable

static volatile uint64_t* g_hpet = (volatile uint64_t*)HPET_BASE_PHYS;
static bool     g_available = false;
static uint64_t g_period_fs = 0;    // counter period in femtoseconds
static uint64_t g_freq_hz   = 0;
static uint32_t g_num_timers = 0;
static uint64_t g_base_counter = 0; // counter value captured at init

static inline uint64_t hpet_rd(uint32_t off) {
    return g_hpet[off / sizeof(uint64_t)];
}
static inline void hpet_wr(uint32_t off, uint64_t val) {
    g_hpet[off / sizeof(uint64_t)] = val;
}

void hpet_init(void) {
    uint64_t caps = hpet_rd(HPET_REG_CAP_ID);

    // Period lives in the top 32 bits (femtoseconds per tick). A sane HPET has
    // a non-zero period below 100 ns (100,000,000 fs); anything else means the
    // block is absent or unmapped, so we decline rather than divide by zero.
    uint64_t period = caps >> 32;
    if (period == 0 || period > 100000000ULL) {
        g_available = false;
        printk(KERN_INFO "HPET: not present (capabilities=0x%llx)\n",
               (unsigned long long)caps);
        return;
    }

    g_period_fs = period;
    g_num_timers = (uint32_t)(((caps >> 8) & 0x1F) + 1);
    g_freq_hz = 1000000000000000ULL / g_period_fs;  // 1e15 fs per second

    // Enable the main counter.
    hpet_wr(HPET_REG_CONFIG, hpet_rd(HPET_REG_CONFIG) | HPET_CFG_ENABLE);
    g_base_counter = hpet_rd(HPET_REG_COUNTER);
    g_available = true;

    printk(ANSI_BRIGHT_GREEN "HPET: " ANSI_RESET
           "High Precision Event Timer online (%llu Hz, %u comparators, period %llu fs)\n",
           (unsigned long long)g_freq_hz, g_num_timers,
           (unsigned long long)g_period_fs);
}

bool hpet_available(void) { return g_available; }

uint64_t hpet_read_counter(void) {
    if (!g_available) return 0;
    return hpet_rd(HPET_REG_COUNTER);
}

uint64_t hpet_get_frequency_hz(void) { return g_freq_hz; }

uint64_t hpet_get_ns(void) {
    if (!g_available) return 0;
    uint64_t ticks = hpet_rd(HPET_REG_COUNTER) - g_base_counter;
    // ns = ticks * period_fs / 1e6 ; group to avoid overflow on large counts.
    return (ticks / 1000000ULL) * g_period_fs + ((ticks % 1000000ULL) * g_period_fs) / 1000000ULL;
}

uint32_t hpet_num_timers(void) { return g_num_timers; }

#else  // Non-x86 targets: HPET is an x86 platform device.

void     hpet_init(void)              { }
bool     hpet_available(void)         { return false; }
uint64_t hpet_read_counter(void)      { return 0; }
uint64_t hpet_get_frequency_hz(void)  { return 0; }
uint64_t hpet_get_ns(void)            { return 0; }
uint32_t hpet_num_timers(void)        { return 0; }

#endif
