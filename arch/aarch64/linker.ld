OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    __kernel_start = .;

    .text.boot : {
        KEEP(*(.text.boot))
    }

    .vectors : ALIGN(2048) {
        KEEP(*(.vectors))
    }

    .text : ALIGN(4096) {
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4096);
        __bss_end = .;
    }

    .stack : ALIGN(4096) {
        . += 0x20000; /* 128 KB dedicated kernel stack */
        stack_top = .;
    }

    __kernel_end = .;
}
