#ifndef _ARCH_X86_64_PIT_H
#define _ARCH_X86_64_PIT_H

#include <stdint.h>

void pit_init(uint32_t frequency);
uint64_t pit_get_ticks(void);
uint64_t pit_get_uptime_ms(void);
void pit_sleep(uint32_t milliseconds);

#endif // _ARCH_X86_64_PIT_H
