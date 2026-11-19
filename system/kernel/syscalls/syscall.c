#include <stdint.h>
#include "../kernel/kernel.h"

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

extern int mouse_x, mouse_y;
extern volatile int mouse_buttons, mouse_click_event;

#define OUT_LINE_H  20
#define OUT_CHAR_W  10

static int out_x0, out_y0, out_cx, out_line;
static spinlock_t out_lock = SPINLOCK_INIT;

void sys_out_begin(int x, int y) {
    uint64_t flags = spin_lock_irqsave(&out_lock);
    out_x0 = x;
    out_y0 = y;
    out_cx = x;
    out_line = 0;
    spin_unlock_irqrestore(&out_lock, flags);
}

int sys_out_lines(void) {
    return out_line + (out_cx > out_x0 ? 1 : 0);
}

static void out_flush(char* seg, int* k) {
    if (*k == 0) return;
    seg[*k] = '\0';
    printf(seg, out_cx, out_y0 + out_line * OUT_LINE_H, 255, 255, 255, 255);
    out_cx += *k * OUT_CHAR_W;
    *k = 0;
}

static int64_t sys_write(const char* s, uint64_t n) {
    if (!user_ptr_ok((uint64_t)s, n)) return -1;
    uint64_t flags = spin_lock_irqsave(&out_lock);
    char seg[96];
    int k = 0;
    for (uint64_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\n') {
            out_flush(seg, &k);
            out_cx = out_x0;
            out_line++;
        } else if ((unsigned char)c >= 32) {
            seg[k++] = c;
            if (k == (int)sizeof(seg) - 1) out_flush(seg, &k);
        }
    }
    out_flush(seg, &k);
    spin_unlock_irqrestore(&out_lock, flags);
    swapBuffers(0);
    return (int64_t)n;
}

static int64_t sys_clear(void) {
    fillScreen(0, 0, 0, 255);
    uint64_t flags = spin_lock_irqsave(&out_lock);
    out_x0 = 15;
    out_y0 = 20;
    out_cx = 15;
    out_line = 0;
    spin_unlock_irqrestore(&out_lock, flags);
    swapBuffers(0);
    return 0;
}

typedef struct __attribute__((packed)) {
    uint32_t a, b, c, d;
    uint8_t  r, g, bl, al;
} PxGfxArgs;

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    if (cr4 & (1ULL << 21)) {
        __asm__ volatile("mov %0, %%cr4" :: "r"(cr4 & ~(1ULL << 21)));
    }

    (void)a3;
    switch (num) {
        case SYS_WRITE:  return sys_write((const char*)a1, a2);
        case SYS_GETKEY: return kbd_wait_char();
        case SYS_CLEAR:  return sys_clear();
        case SYS_DRAW_RECT: {
            if (!user_ptr_ok(a1, sizeof(PxGfxArgs))) return -1;
            const PxGfxArgs* p = (const PxGfxArgs*)a1;
            drawRect(p->a, p->b, p->c, p->d, p->r, p->g, p->bl, p->al);
            return 0;
        }
        case SYS_SWAP:
            swapBuffers(0);
            return 0;
        case SYS_GET_MOUSE: {
            if (!user_ptr_ok(a1, sizeof(PxMouse))) return -1;
            if (a2 == 1) {
                mouse_click_event = 0;
                while (!mouse_click_event) {
                    __asm__ volatile("hlt");
                }
                mouse_click_event = 0;
            } else if (a2 == 2) {
                __asm__ volatile("hlt");
            }
            PxMouse* m = (PxMouse*)a1;
            m->x = (uint32_t)mouse_x;
            m->y = (uint32_t)mouse_y;
            m->buttons = (uint32_t)mouse_buttons;
            return (int64_t)m->buttons;
        }
        case SYS_DRAW_TEXT: {
            if (!user_ptr_ok(a1, sizeof(PxTextArgs))) return -1;
            const PxTextArgs* p = (const PxTextArgs*)a1;
            if (!p->text) return -1;

            char buf[256];
            const char* s = (const char*)p->text;
            uint64_t len = 0;
            while (len < sizeof(buf) - 1 && user_ptr_ok(p->text + len, 1)) {
                buf[len] = s[len];
                if (buf[len] == '\0') break;
                len++;
            }
            buf[len] = '\0';

            printf(buf, p->x, p->y, p->r, p->g, p->b, p->a);
            return 0;
        }
        case SYS_DRAW_LINE: {
            if (!user_ptr_ok(a1, sizeof(PxGfxArgs))) return -1;
            const PxGfxArgs* p = (const PxGfxArgs*)a1;
            drawLine((int)p->a, (int)p->b, (int)p->c, (int)p->d,
                     p->r, p->g, p->bl, p->al);
            return 0;
        }
        case SYS_DRAW_ELLIPSE: {
            if (!user_ptr_ok(a1, sizeof(PxGfxArgs))) return -1;
            const PxGfxArgs* p = (const PxGfxArgs*)a1;
            drawEllipse((int)p->a, (int)p->b, (int)p->c, (int)p->d,
                        p->r, p->g, p->bl, p->al);
            return 0;
        }
        case SYS_MEASURE_TEXT: {
            if (!a1) return 0;
            char buf[256];
            const char* s = (const char*)a1;
            uint64_t len = 0;
            while (len < sizeof(buf) - 1 && user_ptr_ok(a1 + len, 1)) {
                buf[len] = s[len];
                if (buf[len] == '\0') break;
                len++;
            }
            buf[len] = '\0';
            return (int64_t)measure_text(buf);
        }
        case SYS_DRAW_ROUND_RECT: {
            if (!user_ptr_ok(a1, sizeof(PxRoundRectArgs))) return -1;
            const PxRoundRectArgs* p = (const PxRoundRectArgs*)a1;
            drawRoundedRect((int)p->x, (int)p->y, (int)p->w, (int)p->h,
                            (int)p->xRadius, (int)p->yRadius, (int)p->mode,
                            p->r, p->g, p->b, p->a);
            return 0;
        }
        case SYS_DRAW_ARC: {
            if (!user_ptr_ok(a1, sizeof(PxArcArgs))) return -1;
            const PxArcArgs* p = (const PxArcArgs*)a1;
            drawArc((int)p->x, (int)p->y, (int)p->w, (int)p->h,
                    (int)p->startAngle, (int)p->spanAngle,
                    p->r, p->g, p->b, p->a);
            return 0;
        }
        case SYS_DRAW_PIE: {
            if (!user_ptr_ok(a1, sizeof(PxArcArgs))) return -1;
            const PxArcArgs* p = (const PxArcArgs*)a1;
            drawPie((int)p->x, (int)p->y, (int)p->w, (int)p->h,
                    (int)p->startAngle, (int)p->spanAngle,
                    p->r, p->g, p->b, p->a);
            return 0;
        }
        case SYS_DRAW_CHORD: {
            if (!user_ptr_ok(a1, sizeof(PxArcArgs))) return -1;
            const PxArcArgs* p = (const PxArcArgs*)a1;
            drawChord((int)p->x, (int)p->y, (int)p->w, (int)p->h,
                      (int)p->startAngle, (int)p->spanAngle,
                      p->r, p->g, p->b, p->a);
            return 0;
        }
        case SYS_FILL_ELLIPSE: {
            if (!user_ptr_ok(a1, sizeof(PxGfxArgs))) return -1;
            const PxGfxArgs* p = (const PxGfxArgs*)a1;
            fillEllipse((int)p->a, (int)p->b, (int)p->c, (int)p->d,
                        p->r, p->g, p->bl, p->al);
            return 0;
        }
        case SYS_FILL_ROUND_RECT: {
            if (!user_ptr_ok(a1, sizeof(PxRoundRectArgs))) return -1;
            const PxRoundRectArgs* p = (const PxRoundRectArgs*)a1;
            fillRoundedRect((int)p->x, (int)p->y, (int)p->w, (int)p->h,
                            (int)p->xRadius, (int)p->yRadius, (int)p->mode,
                            p->r, p->g, p->b, p->a);
            return 0;
        }
        case SYS_YIELD:
            sched_yield();
            return 0;
        case SYS_GET_TICKS:
            return (int64_t)sched_get_total_ticks();
        default:
            return -1;
    }
}
