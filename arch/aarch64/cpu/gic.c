#include <arch/aarch64/gic.h>
#include <arch/aarch64/io.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define MAX_GIC_IRQS 256

static gic_irq_handler_t irq_handlers[MAX_GIC_IRQS];

void gic_init(void) {
    memset(irq_handlers, 0, sizeof(irq_handlers));

    // 1. Disable Distributor
    mmio_write32(GIC_DIST_BASE + GICD_CTLR, 0);

    // 2. Clear pending and enable default priorities
    uint32_t typer = mmio_read32(GIC_DIST_BASE + GICD_TYPER);
    uint32_t num_irqs = 32 * ((typer & 0x1F) + 1);
    if (num_irqs > MAX_GIC_IRQS) num_irqs = MAX_GIC_IRQS;

    for (uint32_t i = 32; i < num_irqs; i += 32) {
        mmio_write32(GIC_DIST_BASE + GICD_ICENABLER(i / 32), 0xFFFFFFFF);
    }

    for (uint32_t i = 0; i < num_irqs; i += 4) {
        mmio_write32(GIC_DIST_BASE + GICD_IPRIORITYR(i / 4), 0xA0A0A0A0);
    }

    for (uint32_t i = 32; i < num_irqs; i += 4) {
        mmio_write32(GIC_DIST_BASE + GICD_ITARGETSR(i / 4), 0x01010101); // Target Core 0
    }

    // 3. Enable Distributor
    mmio_write32(GIC_DIST_BASE + GICD_CTLR, 1);

    // 4. Enable CPU Interface
    mmio_write32(GIC_CPU_BASE + GICC_PMR, 0xF0); // Priority mask
    mmio_write32(GIC_CPU_BASE + GICC_CTLR, 1);   // Enable signaling

    printk(KERN_INFO "GIC: ARM Generic Interrupt Controller v2 initialized (%u IRQ lines)\n", num_irqs);
}

void gic_enable_irq(uint32_t irq) {
    if (irq >= MAX_GIC_IRQS) return;
    mmio_write32(GIC_DIST_BASE + GICD_ISENABLER(irq / 32), (1U << (irq % 32)));
}

void gic_disable_irq(uint32_t irq) {
    if (irq >= MAX_GIC_IRQS) return;
    mmio_write32(GIC_DIST_BASE + GICD_ICENABLER(irq / 32), (1U << (irq % 32)));
}

void gic_register_handler(uint32_t irq, gic_irq_handler_t handler) {
    if (irq < MAX_GIC_IRQS) {
        irq_handlers[irq] = handler;
        gic_enable_irq(irq);
    }
}

void gic_handle_irq(void) {
    uint32_t iar = mmio_read32(GIC_CPU_BASE + GICC_IAR);
    uint32_t irq = iar & 0x3FF;

    if (irq < 1020) {
        if (irq_handlers[irq]) {
            irq_handlers[irq](irq);
        }
        // End of Interrupt (EOIR)
        mmio_write32(GIC_CPU_BASE + GICC_EOIR, iar);
    }
}
