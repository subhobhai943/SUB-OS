[BITS 64]

extern kernel_main
extern __bss_start
extern __bss_end
extern stack_top

global kernel_entry

section .text
kernel_entry:
    ; 1. Set up high-performance dedicated 64 KB kernel stack
    mov rsp, stack_top

    ; 2. Preserve bootloader arguments safely in callee-saved registers R12 & R13
    mov r12, rdi ; memory_map pointer
    mov r13, rsi ; memory_map count

    ; 3. Zero out the entire BSS section (.bss)
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor al, al
    cld
    rep stosb

    ; 4. Restore bootloader arguments into RDI and RSI per AMD64 System V ABI
    mov rdi, r12
    mov rsi, r13

    ; 5. Call kernel_main(memory_map, memory_map_count)
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
