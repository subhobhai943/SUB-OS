[BITS 64]
default rel

; SUB-OS kernel context switch (x86_64)
; -------------------------------------
; sub_ctx_switch(void** save_sp /* rdi */, void* load_sp /* rsi */)
;
; Saves the SysV callee-saved registers plus RFLAGS of the outgoing thread onto
; its own stack, stashes the resulting stack pointer through *save_sp, then
; loads the incoming thread's stack pointer and unwinds the mirror image. From
; C this looks like an ordinary call that "returns" only when this thread is
; later switched back in. Caller-saved registers are the C caller's problem, as
; usual, so nothing else needs preserving here.

global sub_ctx_switch
global sub_thread_trampoline

extern task_exit

section .text

sub_ctx_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    pushfq

    mov [rdi], rsp          ; *save_sp = outgoing stack pointer
    mov rsp, rsi            ; adopt the incoming stack

    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; First-run trampoline. task_create() forges an initial frame so that the very
; first switch into a fresh thread lands here with r12 = entry, r13 = argument.
; We call the entry function, and if it ever returns we retire the thread.
sub_thread_trampoline:
    mov rdi, r13            ; arg
    call r12               ; entry(arg)
    xor rdi, rdi           ; void entry -> exit code 0
    call task_exit
.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
