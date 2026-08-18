/*
 * SUB-OS AArch64 Exception Vector Table
 * 16 vectors aligned on 128-byte (0x80) boundaries
 */

.macro SAVE_ALL
    sub     sp, sp, #272
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x30, x0, [sp, #240]
    str     x1, [sp, #256]
.endm

.macro RESTORE_ALL
    ldr     x1, [sp, #256]
    msr     spsr_el1, x1
    ldp     x30, x0, [sp, #240]
    msr     elr_el1, x0
    ldp     x28, x29, [sp, #224]
    ldp     x26, x27, [sp, #208]
    ldp     x24, x25, [sp, #192]
    ldp     x22, x23, [sp, #176]
    ldp     x20, x21, [sp, #160]
    ldp     x18, x19, [sp, #144]
    ldp     x16, x17, [sp, #128]
    ldp     x14, x15, [sp, #112]
    ldp     x12, x13, [sp, #96]
    ldp     x10, x11, [sp, #80]
    ldp     x8, x9, [sp, #64]
    ldp     x6, x7, [sp, #48]
    ldp     x4, x5, [sp, #32]
    ldp     x2, x3, [sp, #16]
    ldp     x0, x1, [sp, #0]
    add     sp, sp, #272
    eret
.endm

.section ".vectors", "ax"
.align 11
.global vectors

vectors:
    // 1. Current EL with SP0
    .align 7; b handle_sync_curr_sp0
    .align 7; b handle_irq_curr_sp0
    .align 7; b handle_fiq_curr_sp0
    .align 7; b handle_serror_curr_sp0

    // 2. Current EL with SPx
    .align 7; b handle_sync_curr_spx
    .align 7; b handle_irq_curr_spx
    .align 7; b handle_fiq_curr_spx
    .align 7; b handle_serror_curr_spx

    // 3. Lower EL using AArch64
    .align 7; b handle_sync_lower_aarch64
    .align 7; b handle_irq_lower_aarch64
    .align 7; b handle_fiq_lower_aarch64
    .align 7; b handle_serror_lower_aarch64

    // 4. Lower EL using AArch32
    .align 7; b handle_sync_lower_aarch32
    .align 7; b handle_irq_lower_aarch32
    .align 7; b handle_fiq_lower_aarch32
    .align 7; b handle_serror_lower_aarch32

// Common Vector Handlers
handle_sync_curr_sp0:
handle_sync_curr_spx:
handle_sync_lower_aarch64:
handle_sync_lower_aarch32:
    SAVE_ALL
    mov     x0, sp
    bl      aarch64_sync_handler
    RESTORE_ALL

handle_irq_curr_sp0:
handle_irq_curr_spx:
handle_irq_lower_aarch64:
handle_irq_lower_aarch32:
    SAVE_ALL
    mov     x0, sp
    bl      aarch64_irq_handler
    RESTORE_ALL

handle_fiq_curr_sp0:
handle_fiq_curr_spx:
handle_fiq_lower_aarch64:
handle_fiq_lower_aarch32:
    SAVE_ALL
    mov     x0, sp
    bl      aarch64_fiq_handler
    RESTORE_ALL

handle_serror_curr_sp0:
handle_serror_curr_spx:
handle_serror_lower_aarch64:
handle_serror_lower_aarch32:
    SAVE_ALL
    mov     x0, sp
    bl      aarch64_serror_handler
    RESTORE_ALL
