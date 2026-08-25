#include <mm/vmspace.h>

/*
 * Userland address spaces are not wired up on this architecture yet.
 *
 * The ELF loader and exec path are portable, but they need per-process page
 * tables and a ring-3 entry path that only the x86_64 port implements today.
 * These stubs fail cleanly so a userland exec reports "not supported" instead
 * of silently running a program with kernel privileges.
 */

vmspace_t* vmspace_create(void) { return NULL; }
void vmspace_destroy(vmspace_t* vm) { (void)vm; }

int vmspace_map_page(vmspace_t* vm, uint64_t virt, uint64_t phys, int prot) {
    (void)vm; (void)virt; (void)phys; (void)prot;
    return -1;
}

int vmspace_alloc(vmspace_t* vm, uint64_t virt, size_t len, int prot) {
    (void)vm; (void)virt; (void)len; (void)prot;
    return -1;
}

int vmspace_write(vmspace_t* vm, uint64_t dst, const void* src, size_t len) {
    (void)vm; (void)dst; (void)src; (void)len;
    return -1;
}

uint64_t vmspace_resolve(vmspace_t* vm, uint64_t virt) {
    (void)vm; (void)virt;
    return 0;
}

void vmspace_switch(vmspace_t* vm) { (void)vm; }

bool vmspace_range_ok(vmspace_t* vm, uint64_t virt, size_t len) {
    (void)vm; (void)virt; (void)len;
    return false;
}

uint64_t vmspace_resident_pages(vmspace_t* vm) { (void)vm; return 0; }
