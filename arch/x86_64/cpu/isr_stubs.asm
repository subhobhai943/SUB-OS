[BITS 64]

extern isr_handler_common
global isr_stub_table
global isr_stub_default
global isr_stub_syscall

%macro ISR_NOERRCODE 1
isr_stub_%1:
    push qword 0        ; Dummy error code
    push qword %1       ; Interrupt vector number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
isr_stub_%1:
    push qword %1       ; Interrupt vector number (error code already on stack)
    jmp isr_common_stub
%endmacro

section .text

; CPU Exceptions 0-31
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; Hardware IRQs 32-47
ISR_NOERRCODE 32
ISR_NOERRCODE 33
ISR_NOERRCODE 34
ISR_NOERRCODE 35
ISR_NOERRCODE 36
ISR_NOERRCODE 37
ISR_NOERRCODE 38
ISR_NOERRCODE 39
ISR_NOERRCODE 40
ISR_NOERRCODE 41
ISR_NOERRCODE 42
ISR_NOERRCODE 43
ISR_NOERRCODE 44
ISR_NOERRCODE 45
ISR_NOERRCODE 46
ISR_NOERRCODE 47

; Software syscall gate. Reached by INT 0x80 from ring 3, so its IDT entry is
; installed with DPL 3; the CPU switches to TSS.rsp0 on the way in, which the
; exec path points at a kernel stack owned by the running process.
isr_stub_syscall:
    push qword 0        ; No error code is pushed for a software interrupt
    push qword 128      ; Vector number, matching the IDT slot
    jmp isr_common_stub

isr_stub_default:
    push qword 0
    push qword 255
    jmp isr_common_stub

isr_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    cld
    call isr_handler_common

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

section .data
align 8
isr_stub_table:
    dq isr_stub_0, isr_stub_1, isr_stub_2, isr_stub_3
    dq isr_stub_4, isr_stub_5, isr_stub_6, isr_stub_7
    dq isr_stub_8, isr_stub_9, isr_stub_10, isr_stub_11
    dq isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15
    dq isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19
    dq isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23
    dq isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27
    dq isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31
    dq isr_stub_32, isr_stub_33, isr_stub_34, isr_stub_35
    dq isr_stub_36, isr_stub_37, isr_stub_38, isr_stub_39
    dq isr_stub_40, isr_stub_41, isr_stub_42, isr_stub_43
    dq isr_stub_44, isr_stub_45, isr_stub_46, isr_stub_47
