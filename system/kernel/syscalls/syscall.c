#include <stdint.h>
#include "../kernel/kernel.h"

#define OUT_LINE_H  20
#define OUT_CHAR_W  10

static int out_x0, out_y0, out_cx, out_line;

void sys_out_begin(int x, int y) {
    out_x0 = x;
    out_y0 = y;
    out_cx = x;
    out_line = 0;
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
    swapBuffers(0);
    return (int64_t)n;
}

static int64_t sys_clear(void) {
    fillScreen(0, 0, 0, 255);
    out_x0 = 15;
    out_y0 = 20;
    out_cx = 15;
    out_line = 0;
    swapBuffers(0);
    return 0;
}

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    (void)a3;
    switch (num) {
        case SYS_WRITE:  return sys_write((const char*)a1, a2);
        case SYS_GETKEY: return kbd_wait_char();
        case SYS_CLEAR:  return sys_clear();
        default:         return -1;
    }
}
