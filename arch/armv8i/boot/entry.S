/*
 * SUB-OS ARMv8i (AArch32 / 32-Bit ARMv8) Bootstrap Entry Point
 */

.syntax unified
.arm
.section ".text.boot"
.global _start
.global armv8i_entry

_start:
armv8i_entry:
    // 1. Enable VFP and NEON Coprocessors (CP10 & CP11 Full Access)
    mrc     p15, 0, r0, c1, c0, 2       // Read CPACR
    orr     r0, r0, #(0xF << 20)        // Enable CP10 and CP11
    mcr     p15, 0, r0, c1, c0, 2       // Write CPACR
    isb

    // Enable VFP extension run (FPEXC.EN = 1)
    mov     r0, #(1 << 30)
    vmsr    fpexc, r0

    // 2. Clear SCTLR.V bit (bit 13) to use VBAR for exception vectors
    mrc     p15, 0, r0, c1, c0, 0       // Read SCTLR
    bic     r0, r0, #(1 << 13)          // Clear Hivecs (V=0)
    mcr     p15, 0, r0, c1, c0, 0       // Write SCTLR
    isb

    // 3. Set Vector Base Address Register (VBAR)
    ldr     r0, =vectors
    mcr     p15, 0, r0, c12, c0, 0      // Write VBAR
    isb

    // 4. Setup Banked Stack Pointers for Exception Modes
    cpsid   if, #0x12                   // Switch to IRQ mode
    ldr     sp, =irq_stack_top

    cpsid   if, #0x17                   // Switch to Abort mode
    ldr     sp, =abt_stack_top

    cpsid   if, #0x1B                   // Switch to Undefined mode
    ldr     sp, =und_stack_top

    cpsid   if, #0x13                   // Switch back to Supervisor mode (SVC)
    ldr     sp, =stack_top

    // 5. Zero out BSS section
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
.bss_loop:
    cmp     r0, r1
    strlt   r2, [r0], #4
    blt     .bss_loop

    // 6. Branch to ARMv8i C Kernel Entry
    bl      kernel_main_armv8i

.hang:
    wfi
    b       .hang
