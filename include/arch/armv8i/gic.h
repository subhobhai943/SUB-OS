#ifndef _ARCH_ARMV8I_GIC_H
#define _ARCH_ARMV8I_GIC_H

#include <stdint.h>
#include <stdbool.h>

#define GICD_BASE 0x08000000
#define GICC_BASE 0x08010000

// Distributor Registers
#define GICD_CTLR            (GICD_BASE + 0x000)
#define GICD_TYPER           (GICD_BASE + 0x004)
#define GICD_IIDR            (GICD_BASE + 0x008)
#define GICD_ISENABLER(n)    (GICD_BASE + 0x100 + ((n) * 4))
#define GICD_ICENABLER(n)    (GICD_BASE + 0x180 + ((n) * 4))
#define GICD_ISPENDR(n)      (GICD_BASE + 0x200 + ((n) * 4))
#define GICD_ICPENDR(n)      (GICD_BASE + 0x280 + ((n) * 4))
#define GICD_IPRIORITYR(n)   (GICD_BASE + 0x400 + ((n) * 4))
#define GICD_ITARGETSR(n)    (GICD_BASE + 0x800 + ((n) * 4))
#define GICD_ICFGR(n)        (GICD_BASE + 0xC00 + ((n) * 4))

// CPU Interface Registers
#define GICC_CTLR            (GICC_BASE + 0x000)
#define GICC_PMR             (GICC_BASE + 0x004)
#define GICC_BPR             (GICC_BASE + 0x008)
#define GICC_IAR             (GICC_BASE + 0x00C)
#define GICC_EOIR            (GICC_BASE + 0x010)

typedef void (*gic_irq_handler_t)(uint32_t irq);

void gic_init(void);
void gic_enable_irq(uint32_t irq);
void gic_disable_irq(uint32_t irq);
void gic_set_priority(uint32_t irq, uint8_t prio);
void gic_register_handler(uint32_t irq, gic_irq_handler_t handler);

#endif // _ARCH_ARMV8I_GIC_H
