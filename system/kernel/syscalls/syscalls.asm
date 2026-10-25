[extern syscall_handler]
[global isr_syscall]

isr_syscall:
    push byte 0    ; Ошибка (пустышка)
    push byte 128  ; Номер прерывания (0x80 = 128)
    pusha          ; Сохраняем все регистры общего назначения (EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX)

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10   ; Загружаем селектор данных ядра
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp       ; Передаем указатель на структуру Registers первым аргументом
    call syscall_handler
    mov [esp + 28], eax ; Сохраняем возвращаемое значение (в EAX) перед восстановлением

    pop ecx        ; Очищаем указатель на структуру из стека

    pop gs
    pop fs
    pop es
    pop ds

    popa          ; Восстанавливаем регистры
    add esp, 8    ; Очищаем код ошибки и номер прерывания
    iret          ; Возвращаем управление в пользовательский режим
