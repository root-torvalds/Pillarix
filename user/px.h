#pragma once
#include <stdint.h>

static inline int64_t syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3) {
    int64_t ret;
    __asm__ volatile("int $0x80"
    : "=a"(ret)
    : "a"(n), "D"(a1), "S"(a2), "d"(a3)
    : "rcx", "r11", "memory");
    return ret;
}

static inline void sys_exit(int code) { syscall3(0, (uint64_t)code, 0, 0); }
static inline void sys_write(const char* s, uint64_t len) { syscall3(1, (uint64_t)s, len, 0); }
static inline char sys_getkey(void) { return (char)syscall3(2, 0, 0, 0); }
static inline void sys_clear(void) { syscall3(3, 0, 0, 0); }

static inline void printf(const char* s) {
    uint64_t n = 0;
    while (s[n]) n++;
    sys_write(s, n);
}
