BITS 16
ORG 0x7E00

start2:
    cli
    ; Ensure segment registers are zero
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Save boot drive number passed from stage 1 in DL
    mov [boot_drive], dl

    ; Print Stage 2 startup message
    mov si, msg_stage2_start
    call print_string_16

    ; -------------------------------------------------------------
    ; Step 1: Enable A20 Line
    ; -------------------------------------------------------------
    mov si, msg_a20
    call print_string_16

    in al, 0x92
    test al, 2
    jnz .a20_done
    or al, 2
    and al, 0xFE
    out 0x92, al

.a20_done:
    ; -------------------------------------------------------------
    ; Step 2: Detect Memory via INT 15h, EAX=0xE820
    ; Destination: 0x9000 (count at 0x9000, entries at 0x9008)
    ; -------------------------------------------------------------
    mov si, msg_mmap
    call print_string_16

    ; Zero out count at 0x9000
    mov dword [0x9000], 0
    mov dword [0x9004], 0

    mov di, 0x9008          ; Memory map entries destination
    xor ebx, ebx            ; Continuation value
    xor bp, bp              ; Entry counter
    mov edx, 0x534D4150     ; 'SMAP' signature
    mov eax, 0xE820
    mov ecx, 24             ; Buffer size
    int 0x15
    jc .mmap_error

.mmap_loop:
    inc bp
    add di, 24
    test ebx, ebx
    jz .mmap_finished
    mov edx, 0x534D4150
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .mmap_finished
    jmp .mmap_loop

.mmap_error:
    ; If E820 failed, create 1 fallback entry (128MB usable)
    mov dword [0x9000], 1
    mov dword [0x9004], 0
    mov dword [0x9008], 0x100000       ; base = 1MB
    mov dword [0x900C], 0
    mov dword [0x9010], 0x07F00000     ; length = 127MB
    mov dword [0x9014], 0
    mov dword [0x9018], 1              ; type = 1 (USABLE)
    mov dword [0x901C], 1
    jmp .mmap_store_done

.mmap_finished:
    mov word [0x9000], bp
    mov word [0x9002], 0
    mov dword [0x9004], 0

.mmap_store_done:
    ; -------------------------------------------------------------
    ; Step 3: Load the kernel to 0x100000 (just above 1 MB)
    ;
    ; INT 13h can only land data below 1 MB, and staging the whole kernel in
    ; conventional RAM caps it at roughly 576 KB before the load runs into
    ; video memory at 0xA0000. So each 32 KB chunk is read into one fixed
    ; low buffer at 0x10000 and copied straight up to its final address.
    ;
    ; The copy needs 32-bit offsets from real mode, which is what unreal mode
    ; provides: enter protected mode just long enough to load a 4 GB-limit
    ; descriptor, then drop back. A real-mode segment load afterwards rewrites
    ; only the base and leaves that limit in the descriptor cache.
    ; -------------------------------------------------------------
    mov si, msg_kernel_load
    call print_string_16

    mov word [kernel_dap_off], 0x0000
    mov word [kernel_dap_seg], KERNEL_STAGE_SEG
    mov dword [kernel_dap_lba], 16
    mov dword [kernel_dap_lba + 4], 0
    mov dword [kernel_dest], 0x100000
    mov cx, KERNEL_CHUNKS

.kernel_chunk_loop:
    push cx

    ; Read one chunk into the staging buffer.
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, kernel_dap
    int 0x13
    jc .kernel_disk_error

    ; Relocate it above 1 MB. Unreal mode is re-established every pass, since
    ; the BIOS disk call is free to reload the segment registers.
    call enter_unreal_mode

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov esi, KERNEL_STAGE_ADDR
    mov edi, [kernel_dest]
    mov ecx, (KERNEL_CHUNK_SECTORS * 512) / 4
    a32 rep movsd

    ; Advance the source LBA and the destination address by one chunk.
    add dword [kernel_dap_lba], KERNEL_CHUNK_SECTORS
    add dword [kernel_dest], KERNEL_CHUNK_SECTORS * 512

    pop cx
    loop .kernel_chunk_loop

.kernel_loaded_ok:
    ; -------------------------------------------------------------
    ; Step 4: Set up Identity Paging for Long Mode (All 4 GB)
    ; -------------------------------------------------------------
    ; Clear 24 KB for 6 page tables (0x1000 - 0x6FFF)
    mov ax, 0x0000
    mov es, ax
    mov di, 0x1000
    mov cx, 6144
    xor eax, eax
    rep stosd

    ; PML4[0] -> PDPT at 0x2000
    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0

    ; PDPT -> 4 PDs
    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0
    mov dword [0x2008], 0x4003
    mov dword [0x200C], 0
    mov dword [0x2010], 0x5003
    mov dword [0x2014], 0
    mov dword [0x2018], 0x6003
    mov dword [0x201C], 0

    ; Map 4GB (2048 x 2MB pages)
    mov di, 0x3000
    mov eax, 0x00000083
    mov cx, 2048
.map_4gb_loop:
    mov [di], eax
    mov dword [di + 4], 0
    add eax, 0x200000
    add di, 8
    loop .map_4gb_loop

    ; -------------------------------------------------------------
    ; Step 5: Switch to 32-bit Protected Mode
    ; -------------------------------------------------------------
    mov si, msg_entering_pm
    call print_string_16

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_entry

.kernel_disk_error:
    mov si, msg_kernel_err
    call print_string_16
.halt16:
    cli
    hlt
    jmp .halt16

print_string_16:
    pusha
    mov ah, 0x0E
.p_loop:
    lodsb
    test al, al
    jz .p_done
    int 0x10
    jmp .p_loop
.p_done:
    popa
    ret

; -----------------------------------------------------------------
; 32-Bit Protected Mode Entry Point
; -----------------------------------------------------------------
BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00

    ; The kernel was relocated to 0x100000 chunk by chunk during the load.

    ; Enable PAE in CR4
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; Load CR3
    mov eax, 0x1000
    mov cr3, eax

    ; Enable Long Mode in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Enable Paging and Protected Mode
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    jmp 0x18:lm_entry

; -----------------------------------------------------------------
; 64-Bit Long Mode Entry Point
; -----------------------------------------------------------------
BITS 64
lm_entry:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Arguments: RDI = 0x9008 (entries), RSI = count at 0x9000
    mov rdi, 0x9008
    mov rsi, [0x9000]

    mov rax, 0x100000
    jmp rax

; -----------------------------------------------------------------
; Data Section
; -----------------------------------------------------------------
BITS 16
; Kernel image layout. The image is a 2880-sector floppy and the kernel starts
; at LBA 16, so 44 chunks reads every remaining sector: a 1.375 MB ceiling that
; the loader can grow into without touching this file again.
KERNEL_CHUNK_SECTORS equ 64
KERNEL_CHUNKS        equ 44
KERNEL_STAGE_SEG     equ 0x1000
KERNEL_STAGE_ADDR    equ 0x10000

; -----------------------------------------------------------------------------
; enter_unreal_mode
;
; Leaves the CPU in real mode with DS and ES carrying a 4 GB segment limit, so
; `a32 rep movsd` can reach memory above 1 MB. Clobbers EAX and BX.
; -----------------------------------------------------------------------------
enter_unreal_mode:
    cli
    push ds
    push es

    lgdt [gdt_descriptor]

    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp $+2                 ; Flush the prefetch queue

    mov bx, 0x10            ; Flat 32-bit data descriptor
    mov ds, bx
    mov es, bx

    and al, 0xFE
    mov cr0, eax
    jmp $+2

    ; Popping restores the real-mode segment bases; the 4 GB limits loaded
    ; above stay in the descriptor cache, which is what makes this "unreal".
    pop es
    pop ds
    sti
    ret

align 4
kernel_dest:     dd 0x100000

align 4
boot_drive:      db 0

msg_stage2_start: db "Stage 2: Loaded", 13, 10, 0
msg_a20:         db "Stage 2: A20 enabled", 13, 10, 0
msg_mmap:        db "Stage 2: Memory map detected", 13, 10, 0
msg_kernel_load: db "Stage 2: Loading kernel...", 13, 10, 0
msg_entering_pm: db "Stage 2: Jumping to 64-bit mode...", 13, 10, 0
msg_kernel_err:  db "Stage 2: Kernel Disk Read Error!", 13, 10, 0

align 4
kernel_dap:
    db 0x10
    db 0
    dw KERNEL_CHUNK_SECTORS   ; Sectors per chunk (32 KB)
kernel_dap_off:
    dw 0x0000
kernel_dap_seg:
    dw KERNEL_STAGE_SEG
kernel_dap_lba:
    dq 16

%include "gdt.asm"
