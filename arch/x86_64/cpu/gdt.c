#include <arch/x86_64/gdt.h>
#include <lib/string.h>

extern void gdt_flush(uint64_t gdt_ptr);
extern void tss_flush(void);

static gdt_entry_t gdt_entries[7];
static gdt_ptr_t   gdt_ptr;
static tss_entry_t tss_entry;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

static void write_tss(int num, uint64_t base, uint32_t limit) {
    gdt_set_gate(num, (uint32_t)base, limit, 0x89, 0x00);

    // TSS descriptor takes 16 bytes in 64-bit mode (2 entries)
    uint32_t* high_part = (uint32_t*)&gdt_entries[num + 1];
    high_part[0] = (uint32_t)(base >> 32);
    high_part[1] = 0;
}

void gdt_init(void) {
    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base  = (uint64_t)&gdt_entries;

    // 0x00: Null Descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 0x08: Kernel 64-bit Code Segment (Access=0x9A, Gran=0x20 for Long Mode)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0x20);

    // 0x10: Kernel 64-bit Data Segment (Access=0x92, Gran=0x00)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0x00);

    // 0x18: User 64-bit Code Segment (Access=0xFA, Gran=0x20)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0x20);

    // 0x20: User 64-bit Data Segment (Access=0xF2, Gran=0x00)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0x00);

    // 0x28: TSS (Task State Segment) - takes slots 5 & 6
    extern char stack_top[];
    memset(&tss_entry, 0, sizeof(tss_entry_t));
    tss_entry.rsp0 = (uint64_t)stack_top;
    tss_entry.ist1 = (uint64_t)stack_top;
    tss_entry.iomap_base = sizeof(tss_entry_t);
    write_tss(5, (uint64_t)&tss_entry, sizeof(tss_entry_t) - 1);

    gdt_flush((uint64_t)&gdt_ptr);
    tss_flush();
}

void gdt_set_kernel_stack(uint64_t stack) {
    tss_entry.rsp0 = stack;
}
