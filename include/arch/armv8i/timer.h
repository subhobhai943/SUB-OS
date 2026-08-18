#ifndef _ARCH_ARMV8I_TIMER_H
#define _ARCH_ARMV8I_TIMER_H

#include <stdint.h>

#define ARMV8I_TIMER_VIRT_IRQ 27

void     armv8i_timer_init(uint32_t freq_hz);
uint64_t armv8i_timer_get_ticks(void);
uint64_t armv8i_timer_get_frequency(void);
void     armv8i_timer_handle_irq(void);

#endif // _ARCH_ARMV8I_TIMER_H
