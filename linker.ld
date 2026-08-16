ENTRY(kernel_entry)

SECTIONS
{
    /* Kernel starts at 1MB */
    . = 0x100000;

    _kernel_start = .;

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        PROVIDE(__bss_start = .);
        *(COMMON)
        *(.bss*)
        PROVIDE(__bss_end = .);
    }

    .stack : ALIGN(4K)
    {
        PROVIDE(stack_bottom = .);
        . += 0x10000; /* 64 KB Dedicated Kernel Stack */
        PROVIDE(stack_top = .);
    }

    _kernel_end = .;
}
