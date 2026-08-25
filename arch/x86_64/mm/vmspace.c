#include <mm/vmspace.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <arch/x86_64/paging.h>
#include <kernel/printk.h>
#include <lib/string.h>

/*
 * x86_64 per-process address spaces.
 *
 * The boot page tables identity-map the low 4 GB, so within that window a
 * physical address doubles as a kernel-usable pointer: page tables can be
 * edited and user pages seeded without any temporary mapping window.
 *
 * The PMM allocates linearly from physical zero, so on any machine whose
 * usable memory fits under 4 GB every frame it returns is reachable that way.
 * On a larger machine it can return a frame above the window, which this file
 * has no way to touch -- those are rejected outright rather than silently
 * writing through an unmapped pointer. Lifting that needs either a recursive
 * page-table mapping or a higher-half direct map.
 */

#define KERNEL_PML4_PHYS   0x1000UL
#define PTE_ADDR_MASK      (~0xFFFULL)
#define ENTRIES_PER_TABLE  512

/* Upper bound of the boot identity map; see the file comment. */
#define IDENTITY_MAP_LIMIT (4ULL << 30)

struct vmspace {
    uint64_t pml4_phys;
    uint64_t resident_pages;   /* data pages, excluding page tables */
    uint64_t table_pages;
};

static inline uint64_t* table_at(uint64_t phys) {
    return (uint64_t*)(uintptr_t)phys;
}

static uint64_t arch_flags_of(int prot) {
    uint64_t flags = PAGE_PRESENT;
    if (prot & VM_PROT_WRITE) flags |= PAGE_WRITABLE;
    if (prot & VM_PROT_USER)  flags |= PAGE_USER;
    /* NX is only safe to set once EFER.NXE is on; user text needs it clear
     * anyway, so non-executable pages are simply left W^X-less for now. */
    return flags;
}

/* Take a zeroed frame that the kernel can actually address, or NULL. */
static void* alloc_identity_frame(void) {
    void* page = pmm_alloc_page();
    if (!page) return NULL;
    if ((uint64_t)(uintptr_t)page >= IDENTITY_MAP_LIMIT) {
        pmm_free_page(page);
        printk(KERN_ERR "vmspace: frame at 0x%llx is outside the identity map\n",
               (unsigned long long)(uintptr_t)page);
        return NULL;
    }
    memset(page, 0, VM_PAGE_SIZE);
    return page;
}

static uint64_t* alloc_table(vmspace_t* vm) {
    void* page = alloc_identity_frame();
    if (!page) return NULL;
    if (vm) vm->table_pages++;
    return (uint64_t*)page;
}

vmspace_t* vmspace_create(void) {
    vmspace_t* vm = (vmspace_t*)kzalloc(sizeof(vmspace_t));
    if (!vm) return NULL;

    uint64_t* pml4 = alloc_table(vm);
    if (!pml4) {
        kfree(vm);
        return NULL;
    }

    /*
     * Share the kernel's PML4[0] verbatim. That entry covers the whole low
     * 512 GB, which is where the identity-mapped kernel, its heap and every
     * MMIO window live, so ring 0 keeps working immediately after the CR3
     * load. Userland gets its own slot and never aliases kernel memory.
     */
    uint64_t* kernel_pml4 = table_at(KERNEL_PML4_PHYS);
    pml4[0] = kernel_pml4[0];

    vm->pml4_phys = (uint64_t)(uintptr_t)pml4;
    return vm;
}

/* Walk to the PTE for `virt`, optionally creating the intermediate tables. */
static uint64_t* walk(vmspace_t* vm, uint64_t virt, bool create) {
    uint64_t idx[4] = {
        (virt >> 39) & 0x1FF,
        (virt >> 30) & 0x1FF,
        (virt >> 21) & 0x1FF,
        (virt >> 12) & 0x1FF,
    };

    uint64_t* table = table_at(vm->pml4_phys);
    for (int level = 0; level < 3; level++) {
        uint64_t entry = table[idx[level]];
        if (!(entry & PAGE_PRESENT)) {
            if (!create) return NULL;
            uint64_t* next = alloc_table(vm);
            if (!next) return NULL;
            /* Every level above the leaf must allow user access, or the
             * final PAGE_USER bit is overridden by the walk. */
            entry = (uint64_t)(uintptr_t)next | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
            table[idx[level]] = entry;
        } else if (entry & PAGE_HUGE) {
            /* A large mapping already covers this range; refuse rather than
             * silently splitting the kernel's identity map. */
            return NULL;
        }
        table = table_at(entry & PTE_ADDR_MASK);
    }
    return &table[idx[3]];
}

int vmspace_map_page(vmspace_t* vm, uint64_t virt, uint64_t phys, int prot) {
    if (!vm) return -1;
    if (virt < USER_BASE || virt >= USER_LIMIT) return -1;

    uint64_t* pte = walk(vm, virt & ~(VM_PAGE_SIZE - 1), true);
    if (!pte) return -1;

    if (!(*pte & PAGE_PRESENT)) vm->resident_pages++;
    *pte = (phys & PTE_ADDR_MASK) | arch_flags_of(prot);
    return 0;
}

int vmspace_alloc(vmspace_t* vm, uint64_t virt, size_t len, int prot) {
    if (!vm || len == 0) return -1;

    uint64_t start = virt & ~(VM_PAGE_SIZE - 1);
    uint64_t end   = (virt + len + VM_PAGE_SIZE - 1) & ~(VM_PAGE_SIZE - 1);
    if (end <= start) return -1;

    for (uint64_t page = start; page < end; page += VM_PAGE_SIZE) {
        uint64_t* pte = walk(vm, page, true);
        if (!pte) return -1;

        if (*pte & PAGE_PRESENT) {
            /* Overlapping PT_LOAD segments share a page; widen its rights
             * instead of allocating a second frame over the top. */
            *pte |= arch_flags_of(prot);
            continue;
        }

        void* frame = alloc_identity_frame();
        if (!frame) return -1;

        *pte = ((uint64_t)(uintptr_t)frame & PTE_ADDR_MASK) | arch_flags_of(prot);
        vm->resident_pages++;
    }
    return 0;
}

uint64_t vmspace_resolve(vmspace_t* vm, uint64_t virt) {
    if (!vm) return 0;
    uint64_t* pte = walk(vm, virt & ~(VM_PAGE_SIZE - 1), false);
    if (!pte || !(*pte & PAGE_PRESENT)) return 0;
    return (*pte & PTE_ADDR_MASK) | (virt & (VM_PAGE_SIZE - 1));
}

bool vmspace_range_ok(vmspace_t* vm, uint64_t virt, size_t len) {
    if (!vm || len == 0) return false;
    if (virt < USER_BASE || virt >= USER_LIMIT) return false;
    if (len > USER_LIMIT - virt) return false;

    uint64_t start = virt & ~(VM_PAGE_SIZE - 1);
    uint64_t end   = (virt + len + VM_PAGE_SIZE - 1) & ~(VM_PAGE_SIZE - 1);
    for (uint64_t page = start; page < end; page += VM_PAGE_SIZE) {
        uint64_t* pte = walk(vm, page, false);
        if (!pte || !(*pte & PAGE_PRESENT) || !(*pte & PAGE_USER)) return false;
    }
    return true;
}

int vmspace_write(vmspace_t* vm, uint64_t dst, const void* src, size_t len) {
    if (!vm || !src) return -1;
    const uint8_t* in = (const uint8_t*)src;

    while (len) {
        uint64_t phys = vmspace_resolve(vm, dst);
        if (!phys) return -1;

        size_t offset = (size_t)(dst & (VM_PAGE_SIZE - 1));
        size_t chunk  = VM_PAGE_SIZE - offset;
        if (chunk > len) chunk = len;

        memcpy((void*)(uintptr_t)phys, in, chunk);

        dst += chunk;
        in  += chunk;
        len -= chunk;
    }
    return 0;
}

void vmspace_switch(vmspace_t* vm) {
    uint64_t cr3 = vm ? vm->pml4_phys : KERNEL_PML4_PHYS;
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

uint64_t vmspace_resident_pages(vmspace_t* vm) {
    return vm ? vm->resident_pages : 0;
}

/* Free one level of the private sub-tree rooted at `phys`. */
static void free_level(uint64_t phys, int level) {
    uint64_t* table = table_at(phys);
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        uint64_t entry = table[i];
        if (!(entry & PAGE_PRESENT) || (entry & PAGE_HUGE)) continue;
        if (level > 0) {
            free_level(entry & PTE_ADDR_MASK, level - 1);
        } else {
            pmm_free_page((void*)(uintptr_t)(entry & PTE_ADDR_MASK));
        }
    }
    pmm_free_page((void*)(uintptr_t)phys);
}

void vmspace_destroy(vmspace_t* vm) {
    if (!vm) return;

    uint64_t* pml4 = table_at(vm->pml4_phys);
    /* Slot 0 is the kernel's own table and must outlive this process. */
    uint64_t user_entry = pml4[USER_PML4_SLOT];
    if (user_entry & PAGE_PRESENT) {
        free_level(user_entry & PTE_ADDR_MASK, 2);
    }
    pmm_free_page((void*)(uintptr_t)vm->pml4_phys);
    kfree(vm);
}
