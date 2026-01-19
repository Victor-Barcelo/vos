/* Linker script for VOS kernel */

ENTRY(_start)

SECTIONS
{
    /* Load kernel at 1MB mark (standard for multiboot) */
    . = 1M;

    /* Multiboot header must be first */
    .multiboot BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
    }

    /* Text section (code) */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Read-write data (initialized) */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Read-write data (uninitialized) and stack */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
    }
}
