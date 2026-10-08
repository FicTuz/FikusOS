/* linker.ld — x86 bare metal, entry point at 0x7E00 */

ENTRY(kmain)

SECTIONS
{
    /* Адрес загрузки второй ступени загрузчика */
    . = 0x7E00;

    /* Точка входа должна быть первой в бинарнике */
    .text : ALIGN(16)
    {
        *(.text.entry)   /* секция с _start / kmain, если помечена */
        *(.text*)
    }

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    }

    .data : ALIGN(16)
    {
        *(.data*)
    }

    .bss : ALIGN(16)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
