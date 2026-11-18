#pragma once

#include <stdint.h>

/*
 * Системные вызовы Pillarix. Вызываются из ring 3 командой int 0x80
 *   rax = номер, rdi = arg1, rsi = arg2, rdx = arg3, результат в rax.
 *   Остальные регистры сохраняются.
 */
#define SYS_EXIT   0   /* exit(code)            — завершить программу        */
#define SYS_WRITE  1   /* write(ptr, len)       — вывести текст в терминал   */

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
int      px_install_hello(void);                  /* записать /hello на диск */

/* interrupts.asm: переход в ring 3. Возвращает код выхода программы
 * (или 0xDEAD0000 | номер_вектора, если её убило исключение). */
uint64_t exec_user(uint64_t entry, uint64_t user_rsp);
#define SYS_EXIT   0   /* exit(code)      */
#define SYS_WRITE  1   /* write(ptr, len) */
#define SYS_GETKEY 2   /* getkey() -> ASCII */
#define SYS_CLEAR  3   /* clear()         */

int kbd_wait_char(void);
