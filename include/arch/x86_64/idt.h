#ifndef _ARCH_X86_64_IDT_H
#define _ARCH_X86_64_IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, void* isr, uint8_t flags);
void idt_enable_gate(uint8_t vector);
void idt_disable_gate(uint8_t vector);

#endif // _ARCH_X86_64_IDT_H
