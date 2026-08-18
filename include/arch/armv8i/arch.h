#ifndef _ARCH_ARMV8I_ARCH_H
#define _ARCH_ARMV8I_ARCH_H

#include <stdint.h>
#include <stdbool.h>

#define ARMV8I_PAGE_SIZE 4096

// Mode bits in CPSR
#define CPSR_MODE_USR 0x10
#define CPSR_MODE_FIQ 0x11
#define CPSR_MODE_IRQ 0x12
#define CPSR_MODE_SVC 0x13
#define CPSR_MODE_MON 0x16
#define CPSR_MODE_ABT 0x17
#define CPSR_MODE_HYP 0x1A
#define CPSR_MODE_UND 0x1B
#define CPSR_MODE_SYS 0x1F

#define CPSR_IRQ_DISABLE (1 << 7)
#define CPSR_FIQ_DISABLE (1 << 6)

#endif // _ARCH_ARMV8I_ARCH_H
