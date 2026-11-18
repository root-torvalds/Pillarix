SECTIONS {
    . = 0;
    .text   : { *(.plx_hdr) *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) }
}
