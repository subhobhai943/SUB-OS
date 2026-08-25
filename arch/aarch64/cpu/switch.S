/*
 * SUB-OS kernel context switch (AArch64)
 * --------------------------------------
 * void sub_ctx_switch(void** save_sp, void* load_sp)
 *   x0 = save_sp, x1 = load_sp
 *
 * Saves the AAPCS64 callee-saved registers (x19-x28) plus the frame pointer
 * (x29) and link register (x30) of the outgoing thread, records its stack
 * pointer through *save_sp, then adopts the incoming thread's stack and
 * restores the mirror image. Returns into whoever is switched in.
 *
 * NOTE: implemented for parity with x86_64 and to keep the shared scheduler
 * building on this target; not yet exercised on hardware/QEMU for aarch64.
 */

.text
.globl sub_ctx_switch
.globl sub_thread_trampoline

sub_ctx_switch:
    sub     sp, sp, #(12 * 8)
    stp     x19, x20, [sp, #0]
    stp     x21, x22, [sp, #16]
    stp     x23, x24, [sp, #32]
    stp     x25, x26, [sp, #48]
    stp     x27, x28, [sp, #64]
    stp     x29, x30, [sp, #80]     /* fp, lr */

    mov     x2, sp
    str     x2, [x0]                /* *save_sp = outgoing SP */
    mov     sp, x1                  /* adopt incoming SP      */

    ldp     x19, x20, [sp, #0]
    ldp     x21, x22, [sp, #16]
    ldp     x23, x24, [sp, #32]
    ldp     x25, x26, [sp, #48]
    ldp     x27, x28, [sp, #64]
    ldp     x29, x30, [sp, #80]
    add     sp, sp, #(12 * 8)
    ret

/* Fresh-thread trampoline: x19 = entry, x20 = argument (forged by task_create). */
sub_thread_trampoline:
    mov     x0, x20                 /* arg */
    blr     x19                     /* entry(arg) */
    mov     x0, #0
    bl      task_exit
1:  wfi
    b       1b
