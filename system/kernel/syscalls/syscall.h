#pragma once

#include <stdint.h>

#define SYS_EXIT            0
#define SYS_WRITE           1
#define SYS_GETKEY          2
#define SYS_CLEAR           3
#define SYS_DRAW_RECT       4
#define SYS_DRAW_LINE       5
#define SYS_SWAP            6
#define SYS_GET_MOUSE       7
#define SYS_DRAW_TEXT       8
#define SYS_DRAW_ELLIPSE    9
#define SYS_MEASURE_TEXT    10
#define SYS_DRAW_ROUND_RECT 11
#define SYS_DRAW_ARC        12
#define SYS_DRAW_PIE        13
#define SYS_DRAW_CHORD      14
#define SYS_FILL_ELLIPSE    15
#define SYS_FILL_ROUND_RECT 16
#define SYS_YIELD           17
#define SYS_GET_TICKS       18

#define SYSCALL_VECTOR 0x80
#define PLX_MAGIC 0x30584C50u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t entry;
    uint32_t mem_size;
    uint32_t flags;
} PlxHeader;

int64_t  syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3);
void     sys_out_begin(int x, int y);
int      sys_out_lines(void);

int      user_ptr_ok(uint64_t ptr, uint64_t len);
int      px_run(const char* name, int x, int y);
int      px_run_mode(const char* name, int x, int y, int background);

int kbd_wait_char(void);
