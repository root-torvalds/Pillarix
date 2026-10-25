global jump_to_user_space
section .text

; void jump_to_user_space(uint64_t entry_point, uint64_t user_stack);
; RDI = адрес входа программы (RIP), RSI = вершина стека (RSP)
jump_to_user_space:
    cli

    ; Селекторы сегментов GDT для Ring 3 ( RPL = 3 )
    ; 0x23 - код/данные пользователя (пример селекторов с учетом RPL=3)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Формируем стек для iretq: [SS, RSP, RFLAGS, CS, RIP]
    push 0x23          ; User SS (Stack Segment)
    push rsi           ; User RSP (Указатель на стек)

    push 0x202         ; RFLAGS (включены прерывания IF = 1)

    push 0x1B          ; User CS (Code Segment, 0x18 | 3 = 0x1B)
    push rdi           ; User RIP (Точка входа)

    iretq
