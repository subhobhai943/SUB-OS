; Global Descriptor Table Definition
align 16
gdt_start:
    ; Null descriptor (0x00)
    dq 0

    ; 32-bit Code Segment (0x08)
    dw 0xFFFF       ; Limit (low 16 bits)
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (next 8 bits)
    db 10011010b    ; Access byte (Present, Ring 0, Code/Data, Executable, Read/Write)
    db 11001111b    ; Flags (32-bit, 4KB granularity) and Limit (high 4 bits)
    db 0x00         ; Base (high 8 bits)

    ; 32-bit Data Segment (0x10)
    dw 0xFFFF       ; Limit
    dw 0x0000       ; Base
    db 0x00         ; Base
    db 10010010b    ; Access byte (Present, Ring 0, Code/Data, Read/Write)
    db 11001111b    ; Flags and Limit
    db 0x00         ; Base

    ; 64-bit Code Segment (0x18)
    dw 0x0000       ; Limit (ignored)
    dw 0x0000       ; Base
    db 0x00         ; Base
    db 10011010b    ; Access byte (Present, Ring 0, Code/Data, Executable, Read/Write)
    db 00100000b    ; Flags (64-bit code segment, Long Mode) and Limit
    db 0x00         ; Base

    ; 64-bit Data Segment (0x20)
    dw 0x0000       ; Limit (ignored)
    dw 0x0000       ; Base
    db 0x00         ; Base
    db 10010010b    ; Access byte (Present, Ring 0, Code/Data, Read/Write)
    db 00000000b    ; Flags and Limit
    db 0x00         ; Base

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size of GDT minus 1
    dd gdt_start                ; Base address of GDT
