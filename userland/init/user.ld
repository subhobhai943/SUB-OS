/*
 * Userland link map.
 *
 * Programs are placed inside the PML4 slot the kernel reserves for user
 * mappings (512 GiB), well clear of the identity-mapped low 4 GB where the
 * kernel itself lives. Keep this base in sync with USER_LOAD_BASE in
 * include/mm/vmspace.h.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x8000400000;

    .text : ALIGN(4K) { *(.text*) }
    .rodata : ALIGN(4K) { *(.rodata*) }
    .data : ALIGN(4K) { *(.data*) }
    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
