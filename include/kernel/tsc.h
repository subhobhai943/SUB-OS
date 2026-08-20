#ifndef _KERNEL_TSC_H
#define _KERNEL_TSC_H

#include <stdint.h>
#include <stdbool.h>

void tsc_init(void);
uint64_t tsc_read(void);
uint64_t tsc_get_khz(void);
uint64_t tsc_to_ns(uint64_t cycles);
uint64_t tsc_to_us(uint64_t cycles);
void tsc_udelay(uint32_t microseconds);
void tsc_dump_info(void);

#endif // _KERNEL_TSC_H
