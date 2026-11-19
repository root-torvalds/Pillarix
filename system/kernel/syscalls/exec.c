#include <stdint.h>
#include "../kernel/kernel.h"

#define PX_MAX      (1024 * 1024)       /* до 1 МБ на образ программы */
#define USER_STACK  (64 * 1024)         /* 64 КБ стек программы       */
#define TASK_SLOT_SIZE (PX_MAX + USER_STACK)
#define USER_SLOTS  16

static uint8_t user_slots[USER_SLOTS][TASK_SLOT_SIZE] __attribute__((aligned(4096)));
static uint8_t user_slot_used[USER_SLOTS];
static spinlock_t vm_lock = SPINLOCK_INIT;

int user_ptr_ok(uint64_t ptr, uint64_t len) {
    uint64_t base = 0, size = 0;
    if (!sched_current_user_bounds(&base, &size)) return 0;
    if (len > size || ptr < base) return 0;
    return ptr - base <= size - len;
}

#define PTE_P     1ULL
#define PTE_RW    2ULL
#define PTE_US    4ULL
#define PTE_PS    0x80ULL
#define PTE_NX    (1ULL << 63)
#define PTE_ADDR  0x000FFFFFFFFFF000ULL

#define PT_POOL   128
static uint64_t pt_pool[PT_POOL][512] __attribute__((aligned(4096)));
static int pt_pool_used;

static uint64_t* split_large(uint64_t e, int lvl) {
    if (pt_pool_used >= PT_POOL) return 0;
    uint64_t* t = pt_pool[pt_pool_used++];

    uint64_t base  = e & (lvl == 2 ? 0x000FFFFFC0000000ULL : 0x000FFFFFFFE00000ULL);
    uint64_t step  = (lvl == 2) ? (1ULL << 21) : 4096;
    uint64_t flags = (e & 0xFFF) | (e & PTE_NX);
    if (lvl == 1) flags &= ~PTE_PS;

    for (uint64_t i = 0; i < 512; i++) t[i] = (base + i * step) | flags;
    return t;
}

static int vm_map_user(void* addr, uint64_t len, int exec) {
    uint64_t irq_flags = spin_lock_irqsave(&vm_lock);
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
    spin_unlock_irqrestore(&vm_lock, irq_flags);
    return err;
}

static int alloc_user_slot(void) {
    uint64_t flags = spin_lock_irqsave(&vm_lock);
    for (int i = 0; i < USER_SLOTS; i++) {
        if (!user_slot_used[i]) {
            user_slot_used[i] = 1;
            spin_unlock_irqrestore(&vm_lock, flags);
            return i;
        }
    }
    spin_unlock_irqrestore(&vm_lock, flags);
    return -1;
}

static void free_user_slot(int slot) {
    if (slot < 0 || slot >= USER_SLOTS) return;
    uint64_t flags = spin_lock_irqsave(&vm_lock);
    user_slot_used[slot] = 0;
    spin_unlock_irqrestore(&vm_lock, flags);
}

static int px_err(const char* msg, int x, int y) {
    printf(msg, x, y, 255, 80, 80, 255);
    return 1;
}

int px_run_mode(const char* name, int x, int y, int background) {
    if (name[0] == '\0') return px_err("Usage: ./<program>", x, y);

    int slot = alloc_user_slot();
    if (slot < 0) return px_err("./: no free process slots", x, y);
    uint8_t* mem = user_slots[slot];

    char path[300];
    int j = 0;
    if (name[0] != '/') path[j++] = '/';
    for (int k = 0; name[k] != '\0' && j < 298; k++) path[j++] = name[k];
    path[j] = '\0';

    int n = ext4_read_file_to_buffer(path, mem, PX_MAX);
    if (n < 0)                      { free_user_slot(slot); return px_err("./: no such file", x, y); }
    if (n < (int)sizeof(PlxHeader)) { free_user_slot(slot); return px_err("./: not an executable (too small / empty)", x, y); }

    PlxHeader* h = (PlxHeader*)mem;
    if (h->magic != PLX_MAGIC)      { free_user_slot(slot); return px_err("./: not an executable (bad magic)", x, y); }
    if (h->entry >= (uint32_t)n)    { free_user_slot(slot); return px_err("./: bad entry point", x, y); }
    if (h->mem_size > PX_MAX)       { free_user_slot(slot); return px_err("./: program too big", x, y); }

    uint32_t total = h->mem_size > (uint32_t)n ? h->mem_size : (uint32_t)n;
    for (uint32_t i = (uint32_t)n; i < total; i++) mem[i] = 0;
    for (uint32_t i = PX_MAX; i < TASK_SLOT_SIZE; i++) mem[i] = 0;

    if (vm_map_user(mem, PX_MAX, 1) != 0 ||
        vm_map_user(mem + PX_MAX, USER_STACK, 0) != 0) {
        free_user_slot(slot);
        return px_err("./: cannot map user memory", x, y);
    }

    if (!background) sys_out_begin(x, y);

    int tid = sched_spawn_user(name,
                               (uint64_t)(mem + h->entry),
                               (uint64_t)(mem + TASK_SLOT_SIZE),
                               (uint64_t)mem,
                               TASK_SLOT_SIZE);
    if (tid < 0) {
        free_user_slot(slot);
        return px_err("./: scheduler full", x, y);
    }

    if (background) {
        printf("Started in background", x, y, 0, 255, 100, 255);
        return 1;
    }

    uint64_t code = sched_wait(tid);
    free_user_slot(slot);

    int lines = sys_out_lines();
    if ((uint32_t)code != 0) {
        printf("exit code:", x, y + lines * 20, 255, 255, 0, 255);
        print_hex((uint32_t)code, x + 110, y + lines * 20);
        lines++;
    }
    return lines;
}

int px_run(const char* name, int x, int y) {
    return px_run_mode(name, x, y, 0);
}
