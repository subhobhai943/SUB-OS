/*
 * SUB-OS kernel context switch (ARMv8i / AArch32)
 * -----------------------------------------------
 * void sub_ctx_switch(void** save_sp, void* load_sp)
 *   r0 = save_sp, r1 = load_sp
 *
 * Saves the AAPCS callee-saved registers (r4-r11) plus the link register of
 * the outgoing thread, records its stack pointer through *save_sp, then adopts
 * the incoming thread's stack and restores the mirror image.
 *
 * NOTE: implemented for parity with x86_64 and to keep the shared scheduler
 * building on this target; not yet exercised on hardware/QEMU for armv8i.
 */

.text
.syntax unified
.arm
.globl sub_ctx_switch
.globl sub_thread_trampoline

sub_ctx_switch:
    push    {r4-r11, lr}
    str     sp, [r0]                @ *save_sp = outgoing SP
    mov     sp, r1                  @ adopt incoming SP
    pop     {r4-r11, lr}
    bx      lr

/* Fresh-thread trampoline: r4 = entry, r5 = argument (forged by task_create). */
sub_thread_trampoline:
    mov     r0, r5                  @ arg
    blx     r4                      @ entry(arg)
    mov     r0, #0
    bl      task_exit
1:  b       1b
