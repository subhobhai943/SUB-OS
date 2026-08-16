#ifndef _ARCH_X86_64_PAGING_H
#define _ARCH_X86_64_PAGING_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE       4096
#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITABLE   (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_HUGE       (1ULL << 7)
#define PAGE_NX         (1ULL << 63)

typedef uint64_t pml4e_t;
typedef uint64_t pdpte_t;
typedef uint64_t pde_t;
typedef uint64_t pte_t;

void paging_init(void);
void paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void paging_unmap_page(uint64_t virt);
uint64_t paging_get_phys(uint64_t virt);
void paging_flush_tlb_single(uint64_t virt);

#endif // _ARCH_X86_64_PAGING_H
