[BITS 64]
default rel

; Ring 3 entry and exit for SUB-OS.
;
; user_enter() does not behave like a normal call: it drops to ring 3 and only
; comes back when the process leaves through user_return(), which unwinds
; straight to the frame saved here. From C it therefore looks like a call that
; blocks until the process exits and then returns its exit code.

global user_enter
global user_return

section .text

; int64_t user_enter(uint64_t entry /* rdi */, uint64_t user_rsp /* rsi */)
user_enter:
    cli

    ; Preserve the SysV callee-saved set so user_return can rebuild this frame.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [kernel_resume_rsp], rsp

    ; Ring 3 data selectors: user data descriptor (0x20) with RPL 3.
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build the iretq frame: SS, RSP, RFLAGS, CS, RIP.
    push 0x23                   ; SS  = user data, RPL 3
    push rsi                    ; RSP = top of the user stack
    push 0x202                  ; RFLAGS with IF set, everything else clear
    push 0x1B                   ; CS  = user code (0x18), RPL 3
    push rdi                    ; RIP = ELF entry point

    ; Do not leak kernel register contents across the privilege boundary.
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    iretq

; void user_return(int64_t status /* rdi */) -- never returns to its caller.
user_return:
    cli
    mov rsp, [kernel_resume_rsp]
    mov rax, rdi                ; becomes user_enter's return value

    ; Restore the kernel's own data selectors before touching kernel memory.
    push rax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    pop rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    sti
    ret

section .bss
align 8
kernel_resume_rsp:
    resq 1
