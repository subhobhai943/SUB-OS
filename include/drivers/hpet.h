#ifndef _DRIVERS_HPET_H
#define _DRIVERS_HPET_H

// High Precision Event Timer (HPET) driver.
//
// The HPET is a memory-mapped, free-running up-counter defined by the
// Intel/Microsoft HPET spec and emulated by QEMU at the standard base
// 0xFED00000. It provides a monotonic high-resolution time source far finer
// than the 100 Hz PIT tick.

#include <stdint.h>
#include <stdbool.h>

void     hpet_init(void);
bool     hpet_available(void);
uint64_t hpet_read_counter(void);      // raw main counter value
uint64_t hpet_get_frequency_hz(void);  // counter ticks per second
uint64_t hpet_get_ns(void);            // nanoseconds since HPET was enabled
uint32_t hpet_num_timers(void);        // comparator count reported by the block

#endif // _DRIVERS_HPET_H
