#ifndef _ARCH_AARCH64_GIC_H
#define _ARCH_AARCH64_GIC_H

#include <stdint.h>
#include <stdbool.h>

#define GIC_DIST_BASE 0x08000000
#define GIC_CPU_BASE  0x08010000

// GIC Distributor Registers
#define GICD_CTLR            0x000
#define GICD_TYPER           0x004
#define GICD_ISENABLER(n)   (0x100 + ((n) * 4))
#define GICD_ICENABLER(n)   (0x180 + ((n) * 4))
#define GICD_IPRIORITYR(n)  (0x400 + ((n) * 4))
#define GICD_ITARGETSR(n)   (0x800 + ((n) * 4))
#define GICD_ICFGR(n)       (0xC00 + ((n) * 4))

// GIC CPU Interface Registers
#define GICC_CTLR            0x0000
#define GICC_PMR             0x0004
#define GICC_BPR             0x0008
#define GICC_IAR             0x000C
#define GICC_EOIR            0x0010

typedef void (*gic_irq_handler_t)(uint32_t irq);

void gic_init(void);
void gic_enable_irq(uint32_t irq);
void gic_disable_irq(uint32_t irq);
void gic_register_handler(uint32_t irq, gic_irq_handler_t handler);
void gic_handle_irq(void);

#endif // _ARCH_AARCH64_GIC_H
