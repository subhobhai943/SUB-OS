#include <arch/x86_64/idt.h>
#include <lib/string.h>

extern void idt_flush(uint64_t idt_ptr);

static idt_entry_t idt_entries[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

void idt_set_gate(uint8_t vector, void* isr, uint8_t flags) {
    uint64_t addr = (uint64_t)isr;
    idt_entries[vector].isr_low    = (uint16_t)(addr & 0xFFFF);
    idt_entries[vector].kernel_cs  = 0x08;
    idt_entries[vector].ist        = 0;
    idt_entries[vector].attributes = flags;
    idt_entries[vector].isr_mid    = (uint16_t)((addr >> 16) & 0xFFFF);
    idt_entries[vector].isr_high   = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt_entries[vector].reserved   = 0;
}

void idt_enable_gate(uint8_t vector) {
    idt_entries[vector].attributes |= 0x80;
}

void idt_disable_gate(uint8_t vector) {
    idt_entries[vector].attributes &= ~0x80;
}

void idt_init(void) {
    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uint64_t)&idt_entries;

    memset(&idt_entries, 0, sizeof(idt_entries));
    idt_flush((uint64_t)&idt_ptr);
}
