#include "syscalls.h"
#include "../kernel/kernel.h"
#include <stdint.h>

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
        vga_buffer[index] = (uint16_t)c | (0x0F << 8);
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

#define MAX_OPEN_FILES 16

typedef struct {
    bool active;
    uint32_t inode;
    uint32_t offset;
} FileDescriptor;

static FileDescriptor process_fd_table[MAX_OPEN_FILES];

int32_t sys_write(int fd, const char *buf, uint32_t count) {
    if (fd == 1 || fd == 2) {
        for (uint32_t i = 0; i < count; i++) {
            terminal_putchar(buf[i]);
        }
        return (int32_t)count;
    }

    if (fd >= 3 && fd < MAX_OPEN_FILES && process_fd_table[fd].active) {
        return (int32_t)count;
    }

    return -1;
}

int32_t sys_read(int fd, char *buf, uint32_t count) {
    (void)buf;
    (void)count;
    if (fd == 0) return 0;
    if (fd >= 3 && fd < MAX_OPEN_FILES && process_fd_table[fd].active) return 0;
    return -1;
}

int32_t sys_open(const char *path, int flags) {
    (void)flags;
    uint32_t inode_num = ext4_find_inode_by_path(path);
    if (inode_num == 0) return -1;

    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (!process_fd_table[i].active) {
            process_fd_table[i].active = true;
            process_fd_table[i].inode = inode_num;
            process_fd_table[i].offset = 0;
            return i;
        }
    }
    return -2;
}

int32_t sys_close(int fd) {
    if (fd >= 3 && fd < MAX_OPEN_FILES) {
        process_fd_table[fd].active = false;
        return 0;
    }
    return -1;
}

int32_t sys_getpid(void) {
    return 1;
}

void sys_exit(int code) {
    (void)code;
    while (1) {
        asm volatile("cli; hlt");
    }
}

extern uint32_t *os_framebuffer;
extern void virtio_gpu_redraw(void);

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
        case SYS_GET_FB:
            ret = (int32_t)(uintptr_t)os_framebuffer;
            break;
        case SYS_SWAP:
            swap_buffers(0);
            virtio_gpu_redraw();
            ret = 0;
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
