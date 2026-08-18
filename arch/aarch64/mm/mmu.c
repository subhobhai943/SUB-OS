#include <arch/aarch64/mmu.h>
#include <arch/aarch64/io.h>
#include <lib/string.h>
#include <kernel/printk.h>

// Level 1 Translation Table (PUD / L1, 512 x 1GB entries, root table for 39-bit VA)
static uint64_t __attribute__((aligned(4096))) l1_table[512];
// Level 2 Translation Table (PMD / L2, 512 x 2MB entries for first 1GB: 0x00000000 - 0x3FFFFFFF Device MMIO)
static uint64_t __attribute__((aligned(4096))) l2_dev_table[512];
// Level 2 Translation Table (PMD / L2, 512 x 2MB entries for second 1GB: 0x40000000 - 0x7FFFFFFF RAM)
static uint64_t __attribute__((aligned(4096))) l2_ram_table[512];

void aarch64_mmu_init(void) {
    memset(l1_table, 0, sizeof(l1_table));
    memset(l2_dev_table, 0, sizeof(l2_dev_table));
    memset(l2_ram_table, 0, sizeof(l2_ram_table));

    // L1 Table Entry 0 points to L2 Device Table (0 - 1GB)
    l1_table[0] = ((uint64_t)l2_dev_table) | PTE_VALID | PTE_TABLE;
    // L1 Table Entry 1 points to L2 RAM Table (1GB - 2GB: QEMU virt RAM is 0x40000000)
    l1_table[1] = ((uint64_t)l2_ram_table) | PTE_VALID | PTE_TABLE;

    // 1. Identity map Device MMIO (0x00000000 - 0x3FFFFFFF) as Device-nGnRnE
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t phys = i * 0x200000; // 2MB blocks
        l2_dev_table[i] = phys | PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTRINDX(MT_DEVICE_nGnRnE);
    }

    // 2. Identity map System RAM (0x40000000 - 0x7FFFFFFF, 1GB) as Normal Cacheable
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t phys = 0x40000000 + (i * 0x200000);
        l2_ram_table[i] = phys | PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTRINDX(MT_NORMAL_WB);
    }

    // 3. Set Memory Attribute Indirection Register (MAIR_EL1)
    __asm__ volatile("msr mair_el1, %0" :: "r"((uint64_t)MAIR_VAL));

    // 4. Configure Translation Control Register (TCR_EL1)
    // T0SZ=25 (39-bit VA starting at L1 / 1GB chunks), TG0=0 (4KB granule), Inner/Outer WBWA, Inner Shareable
    uint64_t tcr = (25ULL << 0) |    // T0SZ = 25 (39-bit VA)
                   (1ULL << 8) |     // IRGN0 = Normal WBWA
                   (1ULL << 10) |    // ORGN0 = Normal WBWA
                   (3ULL << 12) |    // SH0 = Inner Shareable
                   (0ULL << 14) |    // TG0 = 4KB
                   (1ULL << 23) |    // EPD1 = Disable TTBR1 translations
                   (2ULL << 32);     // IPS = 40-bit Physical Address (1TB)
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));

    // 5. Load Translation Table Base Register 0 (TTBR0_EL1)
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"((uint64_t)l1_table));

    isb();

    // 6. Enable MMU and Caching in SCTLR_EL1 (M=1, C=1, I=1)
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0) | // M: Enable MMU
             (1 << 2) | // C: Enable Data Cache
             (1 << 12); // I: Enable Instruction Cache
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));

    isb();
}
