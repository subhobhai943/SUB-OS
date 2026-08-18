/*
 * SUB-OS ARMv8i (AArch32) Exception Vector Table
 */

.syntax unified
.arm
.section ".vectors", "ax"
.align 5
.global vectors

vectors:
    ldr     pc, =reset_handler
    ldr     pc, =undef_handler
    ldr     pc, =svc_handler
    ldr     pc, =pabt_handler
    ldr     pc, =dabt_handler
    ldr     pc, =hyp_handler
    ldr     pc, =irq_handler
    ldr     pc, =fiq_handler

.macro SAVE_REGS
    sub     sp, sp, #72
    stmia   sp, {r0 - r12}
    str     lr, [sp, #52]
    mrs     r0, spsr
    str     r0, [sp, #56]
.endm

.macro RESTORE_REGS
    ldr     r0, [sp, #56]
    msr     spsr_cxsf, r0
    ldr     lr, [sp, #52]
    ldmia   sp, {r0 - r12}
    add     sp, sp, #72
.endm

reset_handler:
    b       _start

undef_handler:
    SAVE_REGS
    mov     r0, sp
    bl      armv8i_undef_handler
    RESTORE_REGS
    movs    pc, lr

svc_handler:
    SAVE_REGS
    mov     r0, sp
    bl      armv8i_svc_handler
    RESTORE_REGS
    movs    pc, lr

pabt_handler:
    sub     lr, lr, #4
    SAVE_REGS
    mov     r0, sp
    bl      armv8i_pabt_handler
    RESTORE_REGS
    movs    pc, lr

dabt_handler:
    sub     lr, lr, #8
    SAVE_REGS
    mov     r0, sp
    bl      armv8i_dabt_handler
    RESTORE_REGS
    movs    pc, lr

hyp_handler:
    b       .

irq_handler:
    sub     lr, lr, #4
    SAVE_REGS
    mov     r0, sp
    bl      armv8i_irq_handler
    RESTORE_REGS
    movs    pc, lr

fiq_handler:
    sub     lr, lr, #4
    SAVE_REGS
    mov     r0, sp
    bl      armv8i_fiq_handler
    RESTORE_REGS
    movs    pc, lr
