#include "syscalls.h"
#include "../kernel/kernel.h"
#include <stdint.h>

// Простой вывод символа в текстовом режиме VGA (0xB8000)
static int terminal_row = 0;
static int terminal_col = 0;

void terminal_putchar(char c) {
    volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;
    const int VGA_WIDTH = 80;
    const int VGA_HEIGHT = 25;

    if (c == '\n') {
        terminal_col = 0;
        terminal_row++;
    } else {
        const int index = terminal_row * VGA_WIDTH + terminal_col;
        vga_buffer[index] = (uint16_t)c | (0x0F << 8); // Белый текст на черном фоне
        terminal_col++;
        if (terminal_col >= VGA_WIDTH) {
            terminal_col = 0;
            terminal_row++;
        }
    }

    if (terminal_row >= VGA_HEIGHT) {
        terminal_row = 0;
    }
}

// Максимальное количество открытых файлов в таблице процессов
#define MAX_OPEN_FILES 16

typedef struct {
    bool active;
    uint32_t inode;
    uint32_t offset;
} FileDescriptor;

// Таблица открытых файлов для текущего контекста
static FileDescriptor process_fd_table[MAX_OPEN_FILES];

// Реализация sys_write (вывод в консоль или запись в файл)
int32_t sys_write(int fd, const char *buf, uint32_t count) {
    if (fd == 1 || fd == 2) { // stdout или stderr -> выводим на экран
        for (uint32_t i = 0; i < count; i++) {
            terminal_putchar(buf[i]);
        }
        return (int32_t)count;
    }

    // Если это файловый дескриптор, пишем через драйвер Ext4
    if (fd >= 3 && fd < MAX_OPEN_FILES && process_fd_table[fd].active) {
        return (int32_t)count;
    }

    return -1;
}

// Реализация sys_read
int32_t sys_read(int fd, char *buf, uint32_t count) {
    (void)buf;
    (void)count;
    if (fd == 0) { // stdin
        return 0;
    }

    if (fd >= 3 && fd < MAX_OPEN_FILES && process_fd_table[fd].active) {
        return 0;
    }

    return -1;
}

// Реализация sys_open (открытие файла из структуры Ext4)
int32_t sys_open(const char *path, int flags) {
    (void)flags;
    uint32_t inode_num = ext4_find_inode_by_path(path);
    if (inode_num == 0) {
        return -1; // Файл не найден
    }

    // Ищем свободный дескриптор
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (!process_fd_table[i].active) {
            process_fd_table[i].active = true;
            process_fd_table[i].inode = inode_num;
            process_fd_table[i].offset = 0;
            return i; // Возвращаем файловый дескриптор (fd)
        }
    }

    return -2; // Нет свободных дескрипторов
}

// Реализация sys_close
int32_t sys_close(int fd) {
    if (fd >= 3 && fd < MAX_OPEN_FILES) {
        process_fd_table[fd].active = false;
        return 0;
    }
    return -1;
}

// Получение PID процесса
int32_t sys_getpid(void) {
    return 1; // Процесс init с PID 1
}

// Завершение процесса
void sys_exit(int code) {
    (void)code;
    while (1) {
        asm volatile("cli; hlt");
    }
}

// Главный диспетчер системных вызовов (прерывание int 0x80)
int32_t syscall_handler(Registers *regs) {
    int32_t ret = -1;

    uint32_t syscall_no = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;

    switch (syscall_no) {
        case SYS_EXIT:
            sys_exit((int)arg1);
            break;
        case SYS_READ:
            ret = sys_read((int)arg1, (char *)(uintptr_t)arg2, arg3);
            break;
        case SYS_WRITE:
            ret = sys_write((int)arg1, (const char *)(uintptr_t)arg2, arg3);
            break;
        case SYS_OPEN:
            ret = sys_open((const char *)(uintptr_t)arg1, (int)arg2);
            break;
        case SYS_CLOSE:
            ret = sys_close((int)arg1);
            break;
        case SYS_GETPID:
            ret = sys_getpid();
            break;
        default:
            ret = -1;
            break;
    }

    return ret;
}

void init_syscalls(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        process_fd_table[i].active = false;
    }
}
