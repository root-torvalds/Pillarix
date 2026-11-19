#pragma once
#include <stdint.h>

#define MOUSE_LEFT   1
#define MOUSE_RIGHT  2
#define MOUSE_MIDDLE 4
#define QT_ABSOLUTE_SIZE 0
#define QT_RELATIVE_SIZE 1

typedef struct __attribute__((packed)) {
    uint32_t a, b, c, d;
    uint8_t  r, g, bl, al;
} PxGfxArgs;

typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t buttons;
} PxMouse;

typedef struct __attribute__((packed)) {
    uint64_t text;
    uint32_t x, y;
    uint8_t  r, g, b, a;
} PxTextArgs;

typedef struct __attribute__((packed)) {
    uint32_t x, y, w, h;
    uint32_t xRadius, yRadius, mode;
    uint8_t  r, g, b, a;
} PxRoundRectArgs;

typedef struct __attribute__((packed)) {
    uint32_t x, y, w, h;
    int32_t  startAngle, spanAngle;
    uint8_t  r, g, b, a;
} PxArcArgs;

typedef struct __attribute__((packed)) {
    char    name[256];
    uint8_t file_type;
} PxDirEntry;

typedef struct __attribute__((packed)) {
    uint32_t cpu_id;
    uint32_t task_count;
    uint64_t ticks;
} PxCpuInfo;

typedef struct {
    int id;
    int cpu_id;
    int is_user;
    int state;
    uint64_t rsp, kstack_top, user_mem_base, user_mem_size, exit_code, ticks_used;
    char name[32];
} PxTaskInfo;

static inline int64_t syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3) {
    int64_t ret;
    uint64_t d1, d2, d3;
    __asm__ volatile("int $0x80"
    : "=a"(ret), "=D"(d1), "=S"(d2), "=d"(d3)
    : "a"(n), "1"(a1), "2"(a2), "3"(a3)
    : "rcx", "r8", "r9", "r10", "r11", "memory");
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

static inline uint32_t wait_mouse(PxMouse* m) {
    return (uint32_t)syscall3(7, (uint64_t)m, 2, 0);
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

static inline void drawText(const char* text, uint32_t x, uint32_t y,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxTextArgs args = { (uint64_t)text, x, y, r, g, b, a };
    syscall3(8, (uint64_t)&args, 0, 0);
}

static inline void drawEllipse(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxGfxArgs args = { x, y, w, h, r, g, b, a };
    syscall3(9, (uint64_t)&args, 0, 0);
}

static inline uint32_t measureText(const char* text) {
    return (uint32_t)syscall3(10, (uint64_t)text, 0, 0);
}

static inline void swapBuffers(void) {
    syscall3(6, 0, 0, 0);
}

static inline void drawRoundedRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                   uint32_t xRadius, uint32_t yRadius, uint32_t mode,
                                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxRoundRectArgs args = { x, y, w, h, xRadius, yRadius, mode, r, g, b, a };
    syscall3(11, (uint64_t)&args, 0, 0);
}

static inline void drawArc(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                           int32_t startAngle, int32_t spanAngle,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxArcArgs args = { x, y, w, h, startAngle, spanAngle, r, g, b, a };
    syscall3(12, (uint64_t)&args, 0, 0);
}

static inline void drawPie(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                           int32_t startAngle, int32_t spanAngle,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxArcArgs args = { x, y, w, h, startAngle, spanAngle, r, g, b, a };
    syscall3(13, (uint64_t)&args, 0, 0);
}

static inline void drawChord(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                             int32_t startAngle, int32_t spanAngle,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxArcArgs args = { x, y, w, h, startAngle, spanAngle, r, g, b, a };
    syscall3(14, (uint64_t)&args, 0, 0);
}

static inline void fillEllipse(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxGfxArgs args = { x, y, w, h, r, g, b, a };
    syscall3(15, (uint64_t)&args, 0, 0);
}

static inline void fillRoundedRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                   uint32_t xRadius, uint32_t yRadius, uint32_t mode,
                                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PxRoundRectArgs args = { x, y, w, h, xRadius, yRadius, mode, r, g, b, a };
    syscall3(16, (uint64_t)&args, 0, 0);
}

static inline void sys_yield(void) { syscall3(17, 0, 0, 0); }
static inline uint64_t sys_get_ticks(void) { return (uint64_t)syscall3(18, 0, 0, 0); }

static inline int get_scancode(int blocking) { return (int)syscall3(19, (uint64_t)blocking, 0, 0); }
static inline int fs_read(const char* p, void* b, uint64_t max) { return (int)syscall3(20, (uint64_t)p, (uint64_t)b, max); }
static inline int fs_write(const char* p, const void* b, uint64_t sz) { return (int)syscall3(21, (uint64_t)p, (uint64_t)b, sz); }
static inline int fs_mkdir(const char* p) { return (int)syscall3(22, (uint64_t)p, 0, 0); }
static inline int fs_is_dir(const char* p) { return (int)syscall3(23, (uint64_t)p, 0, 0); }
static inline int fs_list(const char* p, PxDirEntry* out, uint64_t max) { return (int)syscall3(24, (uint64_t)p, (uint64_t)out, max); }
static inline void fs_mkfs(void) { syscall3(25, 0, 0, 0); }
static inline int sys_spawn(const char* p) { return (int)syscall3(26, (uint64_t)p, 0, 0); }
static inline uint64_t sys_wait(int tid) { return (uint64_t)syscall3(27, (uint64_t)tid, 0, 0); }
static inline int sys_ps(PxTaskInfo* out, uint64_t max) { return (int)syscall3(28, (uint64_t)out, max, 0); }
static inline int sys_cores(PxCpuInfo* out, uint64_t max) { return (int)syscall3(29, (uint64_t)out, max, 0); }
static inline void sys_reboot(void) { syscall3(30, 0, 0, 0); }
static inline void sys_shutdown(void) { syscall3(31, 0, 0, 0); }
static inline int fs_remove(const char* p) { return (int)syscall3(32, (uint64_t)p, 0, 0); }
