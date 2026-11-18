[bits 64]
default rel

; Заголовок PLX0 (16 байт)
db 'P', 'L', 'X', '0'
dd _start - $$         ; entry
dd 4096                ; mem_size
dd 0                   ; flags

_start:
    mov eax, 3         ; SYS_CLEAR
    int 0x80

    mov eax, 1         ; SYS_WRITE
    lea rdi, [msg]
    mov rsi, msg_len
    int 0x80

.loop:
    mov eax, 2         ; SYS_GETKEY -> rax = ASCII
    int 0x80
    cmp al, 10         ; Enter?
    je .done

    mov [ch_buf], al
    mov eax, 1         ; SYS_WRITE
    lea rdi, [ch_buf]
    mov rsi, 1
    int 0x80
    jmp .loop

.done:
    mov eax, 1
    lea rdi, [bye]
    mov rsi, bye_len
    int 0x80

    xor edi, edi       ; exit(0)
    xor eax, eax       ; SYS_EXIT
    int 0x80

msg:     db "Pillarix Ring3! Type text (Enter to exit):", 10
msg_len: equ $ - msg
bye:     db 10, "Done!", 10
bye_len: equ $ - bye
ch_buf:  db 0
