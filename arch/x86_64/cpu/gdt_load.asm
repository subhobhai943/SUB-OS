[BITS 64]

global gdt_flush
global tss_flush

section .text
gdt_flush:
    lgdt [rdi]
    ; Reload data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS via far return
    push 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ret

tss_flush:
    mov ax, 0x28
    ltr ax
    ret
