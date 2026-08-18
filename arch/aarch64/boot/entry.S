/*
 * SUB-OS AArch64 / ARMv8 Bootstrap Entry Point
 * Handles EL2/EL3 -> EL1 drop, sets up stack, zeroes BSS, and jumps to kernel_main_aarch64
 */

.section ".text.boot"
.global _start
.global aarch64_entry

_start:
aarch64_entry:
    // Early UART debug ping
    mov     x0, #0x09000000
    mov     w1, #'['
    str     w1, [x0]
    mov     w1, #'S'
    str     w1, [x0]
    mov     w1, #'U'
    str     w1, [x0]
    mov     w1, #'B'
    str     w1, [x0]
    mov     w1, #'-'
    str     w1, [x0]
    mov     w1, #'O'
    str     w1, [x0]
    mov     w1, #'S'
    str     w1, [x0]
    mov     w1, #']'
    str     w1, [x0]
    mov     w1, #'\n'
    str     w1, [x0]

    // 1. Primary Core Check (Core 0 proceeds, other cores halt)
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbz     x0, .primary_core

.secondary_halt:
    wfe
    b       .secondary_halt

.primary_core:
    // 2. Check current Exception Level (EL)
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #2
    b.eq    .from_el2
    cmp     x0, #3
    b.eq    .from_el3
    b       .in_el1

.from_el3:
    // Setup Secure Configuration Register (SCR_EL3)
    mov     x0, #(1 << 10)      // RW=1 (64-bit execution in EL2/EL1)
    orr     x0, x0, #(1 << 0)   // NS=1 (Non-secure)
    msr     scr_el3, x0
    mov     x0, #0b01001        // EL2h mode with DAIF masked
    msr     spsr_el3, x0
    adr     x0, .from_el2
    msr     elr_el3, x0
    eret

.from_el2:
    // Setup Hypervisor Configuration Register (HCR_EL2)
    mov     x0, #(1 << 31)      // RW=1 (64-bit execution in EL1)
    msr     hcr_el2, x0

    // Setup SPSR_EL2 for drop to EL1h (EL1 with dedicated SP_EL1)
    mov     x0, #0b00101        // M[3:0] = 0101 (EL1h)
    orr     x0, x0, #(0xF << 6) // Mask DAIF
    msr     spsr_el2, x0

    adr     x0, .in_el1
    msr     elr_el2, x0
    eret

.in_el1:
    // Enable Floating-Point and Advanced SIMD (NEON) in EL1
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    // 3. Set up Kernel Stack (64 KB dedicated EL1 stack)
    ldr     x0, =stack_top
    mov     sp, x0

    // 4. Enable Vector Table
    ldr     x0, =vectors
    msr     vbar_el1, x0

    // 5. Zero out BSS
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
.bss_loop:
    cmp     x0, x1
    b.ge    .bss_done
    str     xzr, [x0], #8
    b       .bss_loop
.bss_done:

    // 6. Jump to AArch64 C Entry
    bl      kernel_main_aarch64

.hang:
    wfi
    b       .hang
