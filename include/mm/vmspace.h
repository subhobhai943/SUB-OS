#ifndef _MM_VMSPACE_H
#define _MM_VMSPACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Per-process address spaces.
 *
 * The boot page tables identity-map the low 4 GB with 2 MB supervisor pages
 * hanging off PML4[0], and leave PML4[1..511] empty. Userland is therefore
 * given its own PML4 slot rather than carving holes out of the kernel's
 * identity map: a process PML4 shares entry 0 with the kernel (so ring 0 can
 * still reach every kernel address after the CR3 switch) and privately owns
 * entry 1, which is where every user mapping lives.
 */
#define USER_PML4_SLOT    1UL
#define USER_BASE         (USER_PML4_SLOT << 39)               /* 512 GiB      */
#define USER_SPAN         (1UL << 39)                          /* 512 GiB      */
#define USER_LIMIT        (USER_BASE + USER_SPAN)

#define USER_LOAD_BASE    (USER_BASE + 0x400000UL)             /* +4 MiB       */
#define USER_STACK_TOP    (USER_BASE + 0x08000000UL)           /* +128 MiB     */
#define USER_STACK_PAGES  16UL                                 /* 64 KiB stack */

#define VM_PAGE_SIZE      4096UL

/* vmspace_map / vmspace_alloc protection flags (arch-neutral). */
#define VM_PROT_READ      0x1
#define VM_PROT_WRITE     0x2
#define VM_PROT_EXEC      0x4
#define VM_PROT_USER      0x8

typedef struct vmspace vmspace_t;

/* Create an address space that shares the kernel's mappings. NULL on OOM. */
vmspace_t* vmspace_create(void);

/* Release every page and page table the space privately owns. */
void vmspace_destroy(vmspace_t* vm);

/* Map one already-allocated physical page. Returns 0 or a negative errno. */
int vmspace_map_page(vmspace_t* vm, uint64_t virt, uint64_t phys, int prot);

/*
 * Allocate zeroed physical pages and map [virt, virt+len) with them.
 * `virt` and `len` need not be page aligned; the covered range is rounded out.
 */
int vmspace_alloc(vmspace_t* vm, uint64_t virt, size_t len, int prot);

/* Copy kernel memory into an already-mapped user range. */
int vmspace_write(vmspace_t* vm, uint64_t dst, const void* src, size_t len);

/* Translate a user virtual address to a physical one, or 0 if unmapped. */
uint64_t vmspace_resolve(vmspace_t* vm, uint64_t virt);

/* Install this address space on the current CPU. */
void vmspace_switch(vmspace_t* vm);

/* True when the whole range is mapped and reachable from ring 3. */
bool vmspace_range_ok(vmspace_t* vm, uint64_t virt, size_t len);

/* Pages currently charged to this address space. */
uint64_t vmspace_resident_pages(vmspace_t* vm);

#endif /* _MM_VMSPACE_H */
