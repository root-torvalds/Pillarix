[bits 64]
default rel
extern main

section .plx_hdr
    db 'P', 'L', 'X', '0'
    dd _start - $$
    dd 8192
    dd 0

section .text
global _start
_start:
    call main
    mov rdi, rax       ; код возврата main -> SYS_EXIT
    xor eax, eax
    int 0x80
