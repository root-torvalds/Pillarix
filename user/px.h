#pragma once
#include <stdint.h>

#define MOUSE_LEFT   1
#define MOUSE_RIGHT  2
#define MOUSE_MIDDLE 4

typedef struct __attribute__((packed)) {
    uint32_t a, b, c, d;
    uint8_t  r, g, bl, al;
} PxGfxArgs;

typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t buttons;
} PxMouse;

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

static inline uint32_t get_mouse(PxMouse* m) {
    return (uint32_t)syscall3(7, (uint64_t)m, 0, 0);
}

static inline uint32_t wait_click(PxMouse* m) {
    return (uint32_t)syscall3(7, (uint64_t)m, 1, 0);
}

static inline void printf(const char* s) {
    uint64_t n = 0;
    while (s[n]) n++;
    sys_write(s, n);
}

static inline void drawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxGfxArgs args = { x, y, w, h, r, g, b, a };
    syscall3(4, (uint64_t)&args, 0, 0);
}

static inline void drawPoint(uint32_t x, uint32_t y,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    drawRect(x, y, 1, 1, r, g, b, a);
}

static inline void drawLine(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxGfxArgs args = { x1, y1, x2, y2, r, g, b, a };
    syscall3(5, (uint64_t)&args, 0, 0);
}

static inline void fillScreen(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    drawRect(0, 0, 1920, 1080, r, g, b, a);
}

static inline void swapBuffers(void) {
    syscall3(6, 0, 0, 0);
}
