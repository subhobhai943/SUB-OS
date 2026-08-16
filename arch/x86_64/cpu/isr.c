#include <arch/x86_64/isr.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/pic.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <lib/string.h>

extern void* isr_stub_table[48];

static isr_handler_t interrupt_handlers[256];

static const char* exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

void isr_register_handler(uint8_t int_no, isr_handler_t handler) {
    interrupt_handlers[int_no] = handler;
}

void isr_unregister_handler(uint8_t int_no) {
    interrupt_handlers[int_no] = NULL;
}

void isr_handler_common(registers_t* regs) {
    if (regs->int_no < 32) {
        // CPU Exception Fault
        printk(KERN_EMERG "\n[EXCEPTION] Vector %llu: %s (Error code: 0x%llx)\n",
               regs->int_no, exception_messages[regs->int_no], regs->error_code);
        panic_with_regs(exception_messages[regs->int_no], regs);
    }

    if (interrupt_handlers[regs->int_no] != NULL) {
        interrupt_handlers[regs->int_no](regs);
    }

    // Acknowledge Hardware PIC Interrupts (IRQs 32-47)
    if (regs->int_no >= 32 && regs->int_no < 48) {
        pic_send_eoi((uint8_t)(regs->int_no - 32));
    }
}

extern void* isr_stub_default;

void isr_init(void) {
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));

    // 1. Initialize all 256 gates to isr_stub_default
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, isr_stub_default, 0x8E);
    }

    // 2. Install dedicated CPU exception gates (0-31) and hardware IRQ gates (32-47)
    for (uint8_t i = 0; i < 48; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x8E);
    }
}
