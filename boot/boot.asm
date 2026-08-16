BITS 16
ORG 0x7C00

start:
    ; Set up segments and stack
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Save boot drive number from BIOS (DL)
    mov [BOOT_DRIVE], dl

    ; Print greeting
    mov si, MSG_BOOT
    call print_string

    ; Check for LBA extensions (INT 13h, AH=41h)
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc load_chs
    cmp bx, 0xAA55
    jne load_chs

load_lba:
    ; Load stage 2 using LBA (INT 13h, AH=42h)
    ; Read 15 sectors starting from LBA 1 to 0x0000:0x7E00
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, dap
    int 0x13
    jc disk_error
    jmp jump_stage2

load_chs:
    ; Fallback: Load stage 2 using CHS (INT 13h, AH=02h)
    mov ah, 0x02
    mov al, 15              ; 15 sectors (sectors 2-16)
    mov ch, 0               ; Cylinder 0
    mov dh, 0               ; Head 0
    mov cl, 2               ; Sector 2
    xor bx, bx
    mov es, bx
    mov bx, 0x7E00          ; ES:BX = 0x0000:0x7E00
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error

jump_stage2:
    ; Pass boot drive in DL to Stage 2
    mov dl, [BOOT_DRIVE]
    jmp 0x0000:0x7E00

disk_error:
    mov si, MSG_DISK_ERR
    call print_string
.halt:
    cli
    hlt
    jmp .halt

; BIOS Teletype string print (SI = null-terminated string)
print_string:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Data
BOOT_DRIVE   db 0
MSG_BOOT     db "SUB-OS Bootloader...", 13, 10, 0
MSG_DISK_ERR db "Boot Disk Error!", 13, 10, 0

; Disk Address Packet for LBA
align 4
dap:
    db 0x10         ; DAP size (16 bytes)
    db 0            ; Reserved
    dw 15           ; Read 15 sectors
    dw 0x7E00       ; Offset
    dw 0x0000       ; Segment
    dq 1            ; Starting LBA (Sector 1)

; Boot Sector Signature
times 510 - ($ - $$) db 0
dw 0xAA55
