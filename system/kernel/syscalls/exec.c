#include <stdint.h>
#include "../kernel/kernel.h"
#include "hello_bin.h"

#define PX_MAX      (16 * 1024)   /* образ программы */
#define USER_STACK  (16 * 1024)   /* стек программы  */

/* Вся память пользователя: образ в начале, стек в конце. */
static uint8_t user_mem[PX_MAX + USER_STACK] __attribute__((aligned(4096)));

/* rsp ядра на момент запуска программы (см. exec_user в interrupts.asm) */
uint64_t exec_saved_rsp;

int user_ptr_ok(uint64_t ptr, uint64_t len) {
    uint64_t base = (uint64_t)user_mem;
    if (len > sizeof(user_mem) || ptr < base) return 0;
    return ptr - base <= sizeof(user_mem) - len;
}

/* ---- Страницы пользователя ------------------------------------------------
 * UEFI отображает память identity-способом, чаще всего 2 МБ/1 ГБ страницами
 * без бита U/S. Чтобы ring 3 видел ТОЛЬКО свою память, разбиваем большие
 * страницы на 4 КБ и ставим U/S только на страницы user_mem. */
#define PTE_P     1ULL
#define PTE_RW    2ULL
#define PTE_US    4ULL
#define PTE_PS    0x80ULL
#define PTE_NX    (1ULL << 63)
#define PTE_ADDR  0x000FFFFFFFFFF000ULL

#define PT_POOL   4
static uint64_t pt_pool[PT_POOL][512] __attribute__((aligned(4096)));
static int pt_pool_used;

/* e — большая страница уровня lvl (2 = 1 ГБ, 1 = 2 МБ). Возвращает таблицу,
 * которая отображает ту же память страницами уровнем ниже. */
static uint64_t* split_large(uint64_t e, int lvl) {
    if (pt_pool_used >= PT_POOL) return 0;
    uint64_t* t = pt_pool[pt_pool_used++];

    uint64_t base  = e & (lvl == 2 ? 0x000FFFFFC0000000ULL : 0x000FFFFFFFE00000ULL);
    uint64_t step  = (lvl == 2) ? (1ULL << 21) : 4096;
    uint64_t flags = (e & 0xFFF) | (e & PTE_NX);
    if (lvl == 1) flags &= ~PTE_PS;           /* в 4К-записи бит 7 — это PAT */

    for (uint64_t i = 0; i < 512; i++) t[i] = (base + i * step) | flags;
    return t;
}

static int vm_map_user(void* addr, uint64_t len, int exec) {
    uint64_t cr0, cr3;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0 & ~(1ULL << 16)) : "memory");
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t a   = (uint64_t)addr & ~0xFFFULL;
    uint64_t end = (uint64_t)addr + len;
    int err = 0;

    for (; a < end; a += 4096) {
        uint64_t* t = (uint64_t*)(cr3 & PTE_ADDR);
        for (int lvl = 3; lvl >= 0; lvl--) {
            uint64_t idx = (a >> (12 + 9 * lvl)) & 0x1FF;
            uint64_t e = t[idx];
            if (!(e & PTE_P)) { err = -1; goto out; }

            if (lvl >= 1 && lvl <= 2 && (e & PTE_PS)) {
                uint64_t* nt = split_large(e, lvl);
                if (!nt) { err = -1; goto out; }
                e = (uint64_t)nt | PTE_P | PTE_RW | PTE_US;
            } else {
                e |= PTE_RW | PTE_US;
                if (exec) e &= ~PTE_NX;
            }
            t[idx] = e;

            if (lvl == 0) break;
            t = (uint64_t*)(e & PTE_ADDR);
        }
    }
    __asm__ volatile("mov %%cr3, %%rax\n\tmov %%rax, %%cr3" ::: "rax", "memory");

    out:
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
    return err;
}

static int px_err(const char* msg, int x, int y) {
    printf(msg, x, y, 255, 80, 80, 255);
    return 1;
}

/* Команда ./name — загрузить /name с ext4 и выполнить в ring 3. Возвращает
 * число напечатанных строк (как остальные команды терминала). */
int px_run(const char* name, int x, int y) {
    if (name[0] == '\0') return px_err("Usage: ./<program>", x, y);

    char path[300];
    int j = 0;
    if (name[0] != '/') path[j++] = '/';
    for (int k = 0; name[k] != '\0' && j < 298; k++) path[j++] = name[k];
    path[j] = '\0';

    int n = ext4_read_file_to_buffer(path, user_mem, PX_MAX);
    if (n < 0)                       return px_err("./: no such file", x, y);
    if (n < (int)sizeof(PlxHeader))  return px_err("./: not an executable (too small / empty)", x, y);

    PlxHeader* h = (PlxHeader*)user_mem;
    if (h->magic != PLX_MAGIC)       return px_err("./: not an executable (bad magic)", x, y);
    if (h->entry >= (uint32_t)n)     return px_err("./: bad entry point", x, y);
    if (h->mem_size > PX_MAX)        return px_err("./: program too big", x, y);

    /* хвост образа и стек — нули */
    uint32_t total = h->mem_size > (uint32_t)n ? h->mem_size : (uint32_t)n;
    for (uint32_t i = (uint32_t)n; i < total; i++) user_mem[i] = 0;
    for (uint32_t i = PX_MAX; i < sizeof(user_mem); i++) user_mem[i] = 0;

    if (vm_map_user(user_mem, PX_MAX, 1) != 0 ||
        vm_map_user(user_mem + PX_MAX, USER_STACK, 0) != 0)
        return px_err("./: cannot map user memory", x, y);

    sys_out_begin(x, y);
    uint64_t code = exec_user((uint64_t)(user_mem + h->entry),
                              (uint64_t)(user_mem + sizeof(user_mem)));

    int lines = sys_out_lines();
    if ((uint32_t)code != 0) {
        printf("exit code:", x, y + lines * 20, 255, 255, 0, 255);
        print_hex((uint32_t)code, x + 110, y + lines * 20);   /* 0xDEAD00xx = убита исключением xx */
        lines++;
    }
    return lines;
}

/* Записать готовую программу /hello в корень ext4 */
int px_install_hello(void) {
    char dir[300];
    char name[256];
    ext4_split_path("/hello", dir, name);
    return ext4_create_file(dir, name, 0x81ED /* -rwxr-xr-x */, hello_plx, sizeof(hello_plx));
}
