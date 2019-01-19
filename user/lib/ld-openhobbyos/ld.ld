ENTRY(_start)

SECTIONS {
    . = 0x35000000;

    .text ALIGN(4K) : {
        *(.text*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data.rel.ro ALIGN(4K) : {
        *(.data.rel.ro*)
    }

    .data ALIGN(4K) : {
        *(.data*)
    }

    .got ALIGN(4) : {
        *(.got*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss*)
    }

    . = ALIGN(4K);
    _end = .;
}
