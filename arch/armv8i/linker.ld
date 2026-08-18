OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
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
        . += 0x4000; /* 16 KB IRQ Stack */
        irq_stack_top = .;
        . += 0x2000; /* 8 KB ABT Stack */
        abt_stack_top = .;
        . += 0x2000; /* 8 KB UND Stack */
        und_stack_top = .;
        . += 0x10000; /* 64 KB SVC Kernel Stack */
        stack_top = .;
    }

    __kernel_end = .;
}
