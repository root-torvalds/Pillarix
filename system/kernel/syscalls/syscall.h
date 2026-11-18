#pragma once

#include <stdint.h>

/*
 * Системные вызовы Pillarix. Вызываются из ring 3 командой int 0x80
 *   rax = номер, rdi = arg1, rsi = arg2, rdx = arg3, результат в rax.
 *   Остальные регистры сохраняются.
 */
#define SYS_EXIT   0   /* exit(code)      */
#define SYS_WRITE  1   /* write(ptr, len) */
#define SYS_GETKEY 2   /* getkey() -> ASCII */
#define SYS_CLEAR  3   /* clear()         */
#define SYS_DRAW_RECT  4   /* rdi = PxGfxArgs* (x, y, w, h, r, g, b, a)     */
#define SYS_DRAW_LINE  5   /* rdi = PxGfxArgs* (x1, y1, x2, y2, r, g, b, a) */
#define SYS_SWAP       6   /* swapBuffers(0)                                */
#define SYS_GET_MOUSE  7   /* get_mouse(PxMouse* out, int wait_click)       */
#define SYS_DRAW_TEXT     8   /* rdi = PxTextArgs*                             */
#define SYS_DRAW_ELLIPSE  9   /* rdi = PxGfxArgs* (x, y, w, h, r, g, b, a)     */
#define SYS_MEASURE_TEXT  10  /* rdi = const char* -> width in px              */


#define SYSCALL_VECTOR 0x80

/* Формат исполняемого файла PLX0: 16-байтный заголовок + код/данные.
 * Файл загружается в память пользователя целиком, начиная с заголовка.
 * Код должен быть позиционно-независимым (rip-relative). */
#define PLX_MAGIC 0x30584C50u   /* 'P' 'L' 'X' '0' */

typedef struct __attribute__((packed)) {
    uint32_t magic;      /* PLX_MAGIC                                         */
    uint32_t entry;      /* смещение точки входа от начала файла              */
    uint32_t mem_size;   /* сколько байт выделить (хвост после файла = нули)  */
    uint32_t flags;      /* зарезервировано, 0                                */
} PlxHeader;

int64_t  syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3);
void     sys_out_begin(int x, int y);   /* откуда программе выводить текст   */
int      sys_out_lines(void);           /* сколько строк она напечатала      */

int      user_ptr_ok(uint64_t ptr, uint64_t len);  /* [ptr, ptr+len) в памяти программы? */

int      px_run(const char* name, int x, int y);  /* команда ./name          */

/* interrupts.asm: переход в ring 3. Возвращает код выхода программы
 * (или 0xDEAD0000 | номер_вектора, если её убило исключение). */
uint64_t exec_user(uint64_t entry, uint64_t user_rsp);

int kbd_wait_char(void);
