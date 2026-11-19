SECTIONS {
    . = 0;
    .text   : { *(.plx_hdr) *(.text*) }
    .rodata : ALIGN(16) { *(.rodata*) }
    .data   : ALIGN(16) { *(.data*) }
    .bss    : ALIGN(16) { *(.bss*) }
    _end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.got*) *(.plt*) }
}
