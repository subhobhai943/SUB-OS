#ifndef _ARCH_X86_64_ISR_H
#define _ARCH_X86_64_ISR_H

#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

typedef void (*isr_handler_t)(registers_t*);

void isr_init(void);
void isr_register_handler(uint8_t int_no, isr_handler_t handler);
void isr_unregister_handler(uint8_t int_no);

#endif // _ARCH_X86_64_ISR_H
