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
    ; Step 3: Load Kernel from Disk to 0x10000 (Conventional RAM)
    ; 4 chunks x 64 sectors = 256 sectors (128 KB)
    ; -------------------------------------------------------------
    mov si, msg_kernel_load
    call print_string_16

    mov word [kernel_dap_off], 0x0000
    mov word [kernel_dap_seg], 0x1000
    mov dword [kernel_dap_lba], 16
    mov dword [kernel_dap_lba + 4], 0
    mov cx, 4                          ; 4 chunks * 64 sectors = 256 sectors (128 KB)

.kernel_chunk_loop:
    push cx
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, kernel_dap
    int 0x13
    jc .kernel_disk_error

    ; Advance LBA by 64 sectors
    add dword [kernel_dap_lba], 64
    ; Advance Segment by 0x800 (64 sectors * 512 bytes = 32768 bytes = 0x800 paragraphs)
    add word [kernel_dap_seg], 0x0800
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

    ; Copy kernel from 0x10000 to 0x100000 (1MB mark) - 256 KB (512 sectors)
    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, (512 * 512) / 4
    rep movsd

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
    dw 64           ; 64 sectors per chunk (32 KB)
kernel_dap_off:
    dw 0x0000
kernel_dap_seg:
    dw 0x1000
kernel_dap_lba:
    dq 16

%include "gdt.asm"
