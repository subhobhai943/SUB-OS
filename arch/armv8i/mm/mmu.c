#include <arch/armv8i/mmu.h>
#include <arch/armv8i/io.h>
#include <lib/string.h>
#include <kernel/printk.h>

static uint32_t __attribute__((aligned(16384))) l1_page_table[4096];

void armv8i_mmu_init(void) {
    // 1. Identity map the entire 4GB flat space with 1MB sections
    for (uint32_t i = 0; i < 4096; i++) {
        uint32_t phys = i * 0x100000;
        l1_page_table[i] = phys | (3 << 10) | (2 << 0);
    }

    // 2. Set Domain Access Control Register (DACR = 0xFFFFFFFF: All Manager)
    uint32_t dacr = 0xFFFFFFFF;
    __asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(dacr));

    // 3. Set Translation Table Base Control Register (TTBCR = 0)
    uint32_t ttbcr = 0;
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 2" :: "r"(ttbcr));

    // 4. Set Translation Table Base Register 0 (TTBR0)
    uint32_t ttbr0 = (uint32_t)l1_page_table;
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" :: "r"(ttbr0));

    // 5. Invalidate TLB & ICache
    uint32_t zero = 0;
    __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" :: "r"(zero));
    __asm__ volatile("mcr p15, 0, %0, c7, c5, 0" :: "r"(zero));
    isb();
    dsb();

    // 6. Enable MMU and Branch Prediction in SCTLR
    uint32_t sctlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    sctlr |= (1 << 0) | (1 << 11);
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(sctlr));

    isb();
    dsb();
}
