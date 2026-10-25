#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>

#define SYS_EXIT      1
#define SYS_READ      3
#define SYS_WRITE     4
#define SYS_OPEN      5
#define SYS_CLOSE     6
#define SYS_GET_FB    10
#define SYS_SWAP      11

inline int32_t sys_write(int fd, const char *buf, uint32_t count) {
    int32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_WRITE), "b" (fd), "c" (buf), "d" (count)
        : "memory"
    );
    return ret;
}

inline uint32_t* sys_get_fb(void) {
    uint32_t* fb;
    __asm__ volatile (
        "int $0x80"
        : "=a" (fb)
        : "a" (SYS_GET_FB)
        : "memory"
    );
    return fb;
}

inline void sys_swap(void) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a" (SYS_SWAP)
        : "memory"
    );
}

inline void sys_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a" (SYS_EXIT), "b" (code)
        : "memory"
    );
}

#endif
