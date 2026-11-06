#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <stddef.h>

// Номера системных вызовов (как в полноценных ОС)
#define SYS_EXIT      1
#define SYS_READ      3
#define SYS_WRITE     4
#define SYS_OPEN      5
#define SYS_CLOSE     6
#define SYS_GETPID    20
#define SYS_SPAWN     2,  // Запуск нового процесса/программы

// Структура сохраненных регистров при переходе в ядро
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Сохраняются pusha
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) Registers;

void init_syscalls(void);
int32_t syscall_handler(Registers *regs);

#endif
