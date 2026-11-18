#include <stdint.h>
#include "../kernel/kernel.h"

#define GDT_ENTRIES 7   /* 0:null, 1:kcode, 2:kdata, 3:udata, 4:ucode, 5..6:TSS */
static uint64_t gdt[GDT_ENTRIES] __attribute__((aligned(16)));

struct __attribute__((packed)) Tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};
static struct Tss tss;

#define KSTACK_SIZE (32 * 1024)
static uint8_t kstack[KSTACK_SIZE] __attribute__((aligned(16)));

struct __attribute__((packed)) GdtPtr {
    uint16_t limit;
    uint64_t base;
};

void init_gdt(void) {
    __asm__ volatile("cli");

    gdt[0] = 0ULL;
    gdt[1] = 0x00AF9A000000FFFFULL;   /* 0x08: kernel code 64-bit */
    gdt[2] = 0x00CF92000000FFFFULL;   /* 0x10: kernel data */
    gdt[3] = 0x00CFF2000000FFFFULL;   /* 0x18: user data (DPL=3) */
    gdt[4] = 0x00AFFA000000FFFFULL;   /* 0x20: user code 64-bit (DPL=3) */

    uint64_t base  = (uint64_t)&tss;
    uint64_t limit = sizeof(tss) - 1;
    gdt[5] = (limit & 0xFFFF)
    | ((base & 0xFFFFFF) << 16)
    | (0x89ULL << 40)
    | (((limit >> 16) & 0xF) << 48)
    | (((base >> 24) & 0xFF) << 56);
    gdt[6] = base >> 32;

    for (uint32_t i = 0; i < sizeof(tss); i++) ((uint8_t*)&tss)[i] = 0;
    tss.rsp0       = (uint64_t)(kstack + KSTACK_SIZE);
    tss.iomap_base = sizeof(tss);

    struct GdtPtr p = { sizeof(gdt) - 1, (uint64_t)gdt };
    __asm__ volatile(
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        : : "m"(p) : "rax", "memory"
    );
    __asm__ volatile("ltr %0" : : "r"((uint16_t)TSS_SEL));
}
