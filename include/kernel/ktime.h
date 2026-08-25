#ifndef _KERNEL_KTIME_H
#define _KERNEL_KTIME_H

// Unified monotonic high-resolution clock. Prefers the HPET (nanosecond grade)
// and falls back to the 100 Hz timer tick when no HPET is present.

#include <stdint.h>
#include <stdbool.h>

void        ktime_init(void);
uint64_t    ktime_ns(void);          // nanoseconds since ktime_init
uint64_t    ktime_us(void);
uint64_t    ktime_ms(void);
const char* ktime_source(void);      // "HPET" or "PIT"
bool        ktime_is_highres(void);  // true when backed by the HPET

// Busy-wait for the given number of microseconds against the monotonic clock.
// Precise when HPET-backed; used by the compositor's 60 FPS frame pacer.
void        ktime_delay_us(uint64_t us);

#endif // _KERNEL_KTIME_H
