[bits 64]
section .text

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
