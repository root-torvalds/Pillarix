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
#define SYS_GET_SCANCODE    19
#define SYS_FS_READ         20
#define SYS_FS_WRITE        21
#define SYS_FS_MKDIR        22
#define SYS_FS_IS_DIR       23
#define SYS_FS_LIST         24
#define SYS_FS_MKFS         25
#define SYS_SPAWN           26
#define SYS_WAIT            27
#define SYS_PS              28
#define SYS_CORES           29
#define SYS_REBOOT          30
#define SYS_SHUTDOWN        31
#define SYS_FS_REMOVE       32
#define SYS_MAP_FB          33
#define SYS_FB_FLUSH        34
#define SYS_SHM_ALLOC       35
#define SYS_SHM_MAP         36

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
int      px_spawn(const char* name);
uint64_t px_wait(int tid);
int      vm_map_user(void* addr, uint64_t len, int exec);

int kbd_wait_char(void);
