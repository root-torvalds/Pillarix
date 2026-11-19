[bits 64]
default rel

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

    mov rcx, rsp                ; ms_abi arg1 = current rsp
    sub rsp, 32                 ; shadow space (стек уже выровнен по 16)
    call timer_handler_c        ; rax = new_rsp
    mov rsp, rax                ; переключаемся на стек выбранной задачи

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

global sched_yield_asm
extern sched_yield_c

sched_yield_asm:
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

    mov rcx, rsp
    sub rsp, 32
    call sched_yield_c
    mov rsp, rax

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
    mov rax, 0xFEE000B0
    mov dword [rax], 0
    pop rbx
    pop rax
    iretq

; ======================================================================
; Ring 3: системные вызовы и исключения
; ======================================================================
extern syscall_dispatch
extern sched_exit_current

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
    iretq

.exit:
    mov rcx, rdi                ; exit code -> arg1
    sub rsp, 32
    and rsp, ~0xF
    call sched_exit_current
.exit_hang:
    sti
    hlt
    jmp .exit_hang

%macro EXC_NOERR 1
align 16
exc_stub_%1:
    push 0
    push %1
    jmp exception_common
%endmacro

%macro EXC_ERR 1
align 16
exc_stub_%1:
    push %1
    jmp exception_common
%endmacro

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
    mov ecx, 0xDEAD0000
    or  ecx, [rsp]
    sub rsp, 32
    and rsp, ~0xF
    call sched_exit_current
.kernel_fault:
    cli
.hang:
    hlt
    jmp .hang

; ======================================================================
; SMP Трамплин для пробуждения AP-ядер (копируется по физическому адресу 0x8000)
; ======================================================================
global ap_trampoline_start
global ap_trampoline_end
global ap_trampoline_data

[bits 16]
align 16
ap_trampoline_start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    lgdt [0x8000 + (ap_gdt_ptr - ap_trampoline_start)]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp dword 0x08:(0x8000 + (ap_pm32 - ap_trampoline_start))

[bits 32]
ap_pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Включаем PAE + SSE (CR4.PAE = bit 5, OSFXSR = bit 9, OSXMMEXCPT = bit 10)
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    ; Загружаем CR3 от BSP
    mov eax, [0x8000 + (ap_data_cr3 - ap_trampoline_start)]
    mov cr3, eax

    ; Настраиваем IA32_EFER (LME | NXE от BSP)
    mov ecx, 0xC0000080
    mov eax, [0x8000 + (ap_data_efer - ap_trampoline_start)]
    mov edx, [0x8000 + (ap_data_efer - ap_trampoline_start) + 4]
    or eax, (1 << 8)
    wrmsr

    ; Включаем Paging, FPU/SSE (MP, NE) и кэш (сбрасываем CD, NW, EM)
    mov eax, cr0
    and eax, 0x9FFFFFFB
    or eax, 0x80000023
    mov cr0, eax

    jmp 0x18:(0x8000 + (ap_lm64 - ap_trampoline_start))

[bits 64]
ap_lm64:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax

    mov rsp, [0x8000 + (ap_data_stack - ap_trampoline_start)]
    mov ecx, [0x8000 + (ap_data_cpu_id - ap_trampoline_start)]
    sub rsp, 32
    mov rax, [0x8000 + (ap_data_entry - ap_trampoline_start)]
    call rax
.ap_halt:
    cli
    hlt
    jmp .ap_halt

align 8
ap_gdt:
    dq 0x0000000000000000       ; 0x00: null
    dq 0x00CF9A000000FFFF       ; 0x08: 32-bit code
    dq 0x00CF92000000FFFF       ; 0x10: 32-bit data
    dq 0x00AF9A000000FFFF       ; 0x18: 64-bit code
    dq 0x00CF92000000FFFF       ; 0x20: 64-bit data
ap_gdt_end:

ap_gdt_ptr:
    dw ap_gdt_end - ap_gdt - 1
    dd 0x8000 + (ap_gdt - ap_trampoline_start)

align 8
ap_trampoline_data:
ap_data_cr3:    dq 0
ap_data_efer:   dq 0
ap_data_stack:  dq 0
ap_data_entry:  dq 0
ap_data_cpu_id: dd 0
ap_trampoline_end:
