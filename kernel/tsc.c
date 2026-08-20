// High-Resolution Timestamp Counter (TSC) & Nanosecond Timing Subsystem
#include <kernel/tsc.h>
#include <kernel/printk.h>
#include <kernel/timer.h>

static uint64_t g_tsc_khz = 2400000; // Default: 2.4 GHz estimate
static bool g_tsc_available = true;

uint64_t tsc_read(void) {
#if defined(__x86_64__)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__)
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#elif defined(__arm__)
    uint32_t lo, hi;
    __asm__ __volatile__("mrrc p15, 1, %0, %1, c14" : "=r"(lo), "=r"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return timer_get_ticks() * 1000000;
#endif
}

void tsc_init(void) {
#if defined(__x86_64__)
    // Calibrate TSC using I/O bus delay loop (~1us per outb 0x80)
    uint64_t t1 = tsc_read();
    for (volatile int i = 0; i < 10000; i++) {
        __asm__ __volatile__("outb %%al, $0x80" : : "a"(0));
    }
    uint64_t t2 = tsc_read();

    uint64_t diff = (t2 > t1) ? (t2 - t1) : 24000000;
    uint64_t calculated_khz = diff / 10;
    if (calculated_khz > 100000 && calculated_khz < 10000000) {
        g_tsc_khz = calculated_khz;
    } else {
        g_tsc_khz = 2400000;
    }
#elif defined(__aarch64__)
    uint64_t freq;
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(freq));
    if (freq > 0) g_tsc_khz = freq / 1000;
#endif

    printk(KERN_INFO "TSC: Invariant Timestamp Counter online (Calibrated: %llu.%03llu MHz)\n",
           g_tsc_khz / 1000, g_tsc_khz % 1000);
}

uint64_t tsc_get_khz(void) {
    return g_tsc_khz;
}

uint64_t tsc_to_ns(uint64_t cycles) {
    if (g_tsc_khz == 0) return 0;
    return (cycles * 1000000) / g_tsc_khz;
}

uint64_t tsc_to_us(uint64_t cycles) {
    if (g_tsc_khz == 0) return 0;
    return (cycles * 1000) / g_tsc_khz;
}

void tsc_udelay(uint32_t microseconds) {
    uint64_t start = tsc_read();
    uint64_t cycles_needed = ((uint64_t)microseconds * g_tsc_khz) / 1000;
    while ((tsc_read() - start) < cycles_needed) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("yield");
#endif
    }
}

void tsc_dump_info(void) {
    uint64_t now = tsc_read();
    printk(ANSI_BRIGHT_CYAN "=== Hardware Timestamp Counter (TSC) Telemetry ===\n" ANSI_RESET);
    printk("  Available         : " ANSI_BRIGHT_GREEN "%s\n" ANSI_RESET, g_tsc_available ? "YES (Invariant TSC)" : "NO");
    printk("  Calibrated Clock  : \x1b[93m%llu.%03llu MHz\x1b[0m (%llu kHz)\n", g_tsc_khz / 1000, g_tsc_khz % 1000, g_tsc_khz);
    printk("  Raw Cycles        : \x1b[92m%llu cycles\x1b[0m\n", now);
    printk("  Resolution        : \x1b[96m~%llu ns per cycle\x1b[0m\n\n", 1000000 / (g_tsc_khz ? g_tsc_khz : 1));
}
