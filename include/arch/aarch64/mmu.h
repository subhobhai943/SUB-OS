#ifndef _ARCH_AARCH64_MMU_H
#define _ARCH_AARCH64_MMU_H

#include <stdint.h>
#include <stdbool.h>

#define MMU_PAGE_SIZE 4096

// Memory Types / Attributes index for MAIR_EL1
#define MT_DEVICE_nGnRnE 0
#define MT_NORMAL_NC     1
#define MT_NORMAL_WB     2

#define MAIR_DEVICE_nGnRnE 0x00
#define MAIR_NORMAL_NC     0x44
#define MAIR_NORMAL_WB     0xFF

#define MAIR_VAL ((MAIR_DEVICE_nGnRnE << (MT_DEVICE_nGnRnE * 8)) | \
                  (MAIR_NORMAL_NC     << (MT_NORMAL_NC     * 8)) | \
                  (MAIR_NORMAL_WB     << (MT_NORMAL_WB     * 8)))

// Page descriptor bits
#define PTE_VALID     (1ULL << 0)
#define PTE_TABLE     (1ULL << 1)
#define PTE_BLOCK     (0ULL << 1)
#define PTE_USER      (1ULL << 6)
#define PTE_RO        (1ULL << 7)
#define PTE_SH_INNER  (3ULL << 8)
#define PTE_AF        (1ULL << 10)
#define PTE_NG        (1ULL << 11)
#define PTE_PXN       (1ULL << 53)
#define PTE_UXN       (1ULL << 54)

#define PTE_ATTRINDX(x) (((uint64_t)(x)) << 2)

void aarch64_mmu_init(void);
void aarch64_mmu_map_2mb(uint64_t virt_addr, uint64_t phys_addr, uint32_t mem_type);

#endif // _ARCH_AARCH64_MMU_H
