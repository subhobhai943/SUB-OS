#ifndef _ARCH_ARMV8I_MMU_H
#define _ARCH_ARMV8I_MMU_H

#include <stdint.h>

// VMSAv7 1MB Section Descriptor Flags
#define SECTION_TYPE_MASK      (3 << 0)
#define SECTION_TYPE_FAULT     (0 << 0)
#define SECTION_TYPE_SECTION   (2 << 0)
#define SECTION_BUFFERABLE     (1 << 2)
#define SECTION_CACHEABLE      (1 << 3)
#define SECTION_AP_RW_ALL      (3 << 10)
#define SECTION_TEX_DEVICE     (0 << 12)
#define SECTION_TEX_NORMAL     (1 << 12)
#define SECTION_SHAREABLE      (1 << 16)
#define SECTION_NON_GLOBAL     (1 << 17)

void armv8i_mmu_init(void);

#endif // _ARCH_ARMV8I_MMU_H
