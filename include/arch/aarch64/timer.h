#ifndef _ARCH_AARCH64_TIMER_H
#define _ARCH_AARCH64_TIMER_H

#include <stdint.h>

#define ARCH_TIMER_VIRT_IRQ 27

void     aarch64_timer_init(uint32_t freq_hz);
uint64_t aarch64_timer_get_ticks(void);
uint64_t aarch64_timer_get_freq(void);
void     aarch64_timer_handle_irq(void);

#endif // _ARCH_AARCH64_TIMER_H
