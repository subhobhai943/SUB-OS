#include <arch/armv8i/gic.h>
#include <arch/armv8i/io.h>
#include <kernel/printk.h>

void gic_init(void) {
    // 1. Disable Distributor and CPU interface during configuration
    mmio_write32(GICD_CTLR, 0);
    mmio_write32(GICC_CTLR, 0);

    // 2. Query lines count (TYPER ITLinesNumber)
    uint32_t typer = mmio_read32(GICD_TYPER);
    uint32_t lines = 32 * ((typer & 0x1F) + 1);

    // 3. Disable all interrupts & clear pending
    for (uint32_t i = 0; i < lines / 32; i++) {
        mmio_write32(GICD_ICENABLER(i), 0xFFFFFFFF);
        mmio_write32(GICD_ICPENDR(i), 0xFFFFFFFF);
    }

    // 4. Default priority 0xA0 and target CPU 0 (0x01)
    for (uint32_t i = 0; i < lines / 4; i++) {
        mmio_write32(GICD_IPRIORITYR(i), 0xA0A0A0A0);
    }
    for (uint32_t i = 8; i < lines / 4; i++) {
        mmio_write32(GICD_ITARGETSR(i), 0x01010101);
    }

    // 5. Enable Distributor (Group 0 & 1)
    mmio_write32(GICD_CTLR, 3);

    // 6. Set CPU Interface Priority Mask to allow all interrupts (0xF0)
    mmio_write32(GICC_PMR, 0xF0);

    // 7. Enable CPU Interface (Group 0 & 1)
    mmio_write32(GICC_CTLR, 3);

    // 8. Unmask Virtual Timer PPI 27
    gic_enable_irq(27);

    printk(KERN_INFO "GIC: ARM Generic Interrupt Controller v2 initialized (%u IRQ lines)\n", lines);
}

void gic_enable_irq(uint32_t irq) {
    uint32_t reg_idx = irq / 32;
    uint32_t bit_idx = irq % 32;
    mmio_write32(GICD_ISENABLER(reg_idx), (1 << bit_idx));
}

void gic_disable_irq(uint32_t irq) {
    uint32_t reg_idx = irq / 32;
    uint32_t bit_idx = irq % 32;
    mmio_write32(GICD_ICENABLER(reg_idx), (1 << bit_idx));
}

void gic_set_priority(uint32_t irq, uint8_t prio) {
    uint32_t reg_idx = irq / 4;
    uint32_t shift = (irq % 4) * 8;
    uint32_t val = mmio_read32(GICD_IPRIORITYR(reg_idx));
    val &= ~(0xFF << shift);
    val |= ((uint32_t)prio << shift);
    mmio_write32(GICD_IPRIORITYR(reg_idx), val);
}
