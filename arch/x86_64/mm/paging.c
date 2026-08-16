#include <arch/x86_64/paging.h>
#include <mm/pmm.h>
#include <lib/string.h>

static pml4e_t* kernel_pml4 = (pml4e_t*)0x1000;

void paging_flush_tlb_single(uint64_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void paging_init(void) {
    // PML4 is already loaded at 0x1000 by Stage 2 bootloader with 4GB mapped
    kernel_pml4 = (pml4e_t*)0x1000;
}

void paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(kernel_pml4[pml4_idx] & PAGE_PRESENT)) {
        void* new_pdpt = pmm_alloc_page();
        if (!new_pdpt) return;
        memset(new_pdpt, 0, PAGE_SIZE);
        kernel_pml4[pml4_idx] = (uint64_t)new_pdpt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    }

    pdpte_t* pdpt = (pdpte_t*)(kernel_pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        void* new_pd = pmm_alloc_page();
        if (!new_pd) return;
        memset(new_pd, 0, PAGE_SIZE);
        pdpt[pdpt_idx] = (uint64_t)new_pd | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    }

    pde_t* pd = (pde_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        void* new_pt = pmm_alloc_page();
        if (!new_pt) return;
        memset(new_pt, 0, PAGE_SIZE);
        pd[pd_idx] = (uint64_t)new_pt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    }

    pte_t* pt = (pte_t*)(pd[pd_idx] & ~0xFFFULL);
    pt[pt_idx] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;

    paging_flush_tlb_single(virt);
}

void paging_unmap_page(uint64_t virt) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(kernel_pml4[pml4_idx] & PAGE_PRESENT)) return;
    pdpte_t* pdpt = (pdpte_t*)(kernel_pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return;
    pde_t* pd = (pde_t*)(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return;
    pte_t* pt = (pte_t*)(pd[pd_idx] & ~0xFFFULL);

    pt[pt_idx] = 0;
    paging_flush_tlb_single(virt);
}

uint64_t paging_get_phys(uint64_t virt) {
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(kernel_pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    pdpte_t* pdpt = (pdpte_t*)(kernel_pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    pde_t* pd = (pde_t*)(pdpt[pdpt_idx] & ~0xFFFULL);

    // Check for 2MB huge page
    if (pd[pd_idx] & PAGE_HUGE) {
        return (pd[pd_idx] & ~0x1FFFFFULL) | (virt & 0x1FFFFF);
    }

    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    pte_t* pt = (pte_t*)(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFFULL) | (virt & 0xFFF);
}
