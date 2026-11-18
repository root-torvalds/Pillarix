[bits 64]

global keyboard_handler_asm
extern keyboard_handler_c

keyboard_handler_asm:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rbp, rsp
    sub rsp, 40
    and rsp, ~0xF

    call keyboard_handler_c

    mov rsp, rbp

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

global mouse_handler_asm
extern mouse_handler_c

mouse_handler_asm:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rbp, rsp
    sub rsp, 40
    and rsp, ~0xF

    call mouse_handler_c

    mov rsp, rbp

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

global timer_handler_asm
extern timer_handler_c

timer_handler_asm:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rbp, rsp
    sub rsp, 40
    and rsp, ~0xF

    call timer_handler_c

    mov rsp, rbp

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

global dummy_handler_asm
dummy_handler_asm:
    push rax
    push rbx
    ; Отправка EOI в Local APIC прямо из ассемблера (адрес 0xFEE000B0)
    mov rax, 0xFEE000B0
    mov dword [rax], 0
    pop rbx
    pop rax
    iretq

; ======================================================================
; Ring 3: вход в пользовательскую программу, системные вызовы, исключения
; ======================================================================
default rel
extern syscall_dispatch
extern exec_saved_rsp

; uint64_t exec_user(uint64_t entry, uint64_t user_rsp)   — ms_abi: rcx, rdx
; Сохраняет контекст ядра и через iretq прыгает в ring 3 (CS=0x23, SS=0x1B).
global exec_user
exec_user:
    push rbx
    push rbp
    push rdi
    push rsi
    push r12
    push r13
    push r14
    push r15
    mov [rel exec_saved_rsp], rsp

    push 0x1B                   ; SS  = USER_DS (0x18 | 3)
    push rdx                    ; RSP программы
    push 0x202                  ; RFLAGS: IF = 1
    push 0x23                   ; CS  = USER_CS (0x20 | 3)
    push rcx                    ; RIP = точка входа                   ; RIP = точка входа

    xor eax, eax                ; не отдаём программе регистры ядра
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d
    iretq

; Возврат из программы в ядро (SYS_EXIT или исключение): rax = код возврата.
; Кадр прерывания на ядерном стеке просто выбрасываем.
exec_done:
    mov rsp, [rel exec_saved_rsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbp
    pop rbx
    sti
    ret

; int 0x80 из ring 3 (шлюз DPL=3). Стек уже ядерный (TSS.rsp0).
global syscall_handler_asm
syscall_handler_asm:
    test rax, rax
    jz .exit

    push rbp
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov rbp, rsp
    sub rsp, 32
    and rsp, ~0xF

    mov r8, rsi                 ; arg2
    mov r9, rdx                 ; arg3
    mov rdx, rdi                ; arg1
    mov rcx, rax                ; номер
    call syscall_dispatch       ; результат в rax

    mov rsp, rbp
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbp
    iretq                       ; обратно в ring 3

.exit:
    mov rax, rdi                ; SYS_EXIT(code)
    jmp exec_done

; ---- Исключения CPU (векторы 0..31) ----------------------------------
; Если исключение случилось в ring 3 — убиваем программу и возвращаемся в
; ядро с кодом 0xDEAD0000 | вектор. Если в ядре — останавливаемся.
%macro EXC_NOERR 1
align 16
exc_stub_%1:
    push 0                      ; фиктивный код ошибки, чтобы кадр был единым
    push %1
    jmp exception_common
%endmacro

%macro EXC_ERR 1
align 16
exc_stub_%1:
    push %1                     ; код ошибки CPU уже на стеке
    jmp exception_common
%endmacro

; Заглушки лежат подряд по 16 байт: адрес вектора n = exception_stubs + 16*n
global exception_stubs
align 16
exception_stubs:
EXC_NOERR 0
EXC_NOERR 1
EXC_NOERR 2
EXC_NOERR 3
EXC_NOERR 4
EXC_NOERR 5
EXC_NOERR 6
EXC_NOERR 7
EXC_ERR 8
EXC_NOERR 9
EXC_ERR 10
EXC_ERR 11
EXC_ERR 12
EXC_ERR 13
EXC_ERR 14
EXC_NOERR 15
EXC_NOERR 16
EXC_ERR 17
EXC_NOERR 18
EXC_NOERR 19
EXC_NOERR 20
EXC_ERR 21
EXC_NOERR 22
EXC_NOERR 23
EXC_NOERR 24
EXC_NOERR 25
EXC_NOERR 26
EXC_NOERR 27
EXC_NOERR 28
EXC_ERR 29
EXC_ERR 30
EXC_NOERR 31

exception_common:
    ; [rsp] = вектор, [rsp+8] = код ошибки, [rsp+16] = rip, [rsp+24] = cs
    test qword [rsp + 24], 3
    jz .kernel_fault
    mov eax, 0xDEAD0000
    or  eax, [rsp]
    jmp exec_done
.kernel_fault:
    cli
.hang:
    hlt
    jmp .hang
