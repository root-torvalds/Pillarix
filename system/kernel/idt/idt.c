#include <stdint.h>
#include "../kernel/kernel.h"

struct IDTEntry {
    uint16_t offset_low;
    uint16_t segment_selector;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct IDTEntry idt[256];

extern void dummy_handler_asm(void);
extern void timer_handler_asm(void);
extern void sched_yield_asm(void);
extern void mouse_handler_asm(void);
extern void keyboard_handler_asm(void);
extern void syscall_handler_asm(void);
extern uint8_t exception_stubs[];

static void set_gate(int v, uint64_t handler, uint8_t attributes) {
    idt[v].offset_low       = (uint16_t)(handler & 0xFFFF);
    idt[v].offset_mid       = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[v].offset_high      = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[v].segment_selector = KERNEL_CS;
    idt[v].ist              = 0;
    idt[v].attributes       = attributes;
    idt[v].reserved         = 0;
}

void init_idt(void) {
    uint64_t dummy_address = (uint64_t)dummy_handler_asm;

    for (int i = 0; i < 256; i++) {
        set_gate(i, dummy_address, 0x8E);
    }

    // Таймер LAPIC (вектор 32) — переключает задачи на каждом ядре
    set_gate(32, (uint64_t)timer_handler_asm, 0x8E);

    // Клавиатура (вектор 33) и мышь (вектор 0x2C)
    set_gate(33, (uint64_t)keyboard_handler_asm, 0x8E);
    set_gate(0x2C, (uint64_t)mouse_handler_asm, 0x8E);

    // Исключения CPU (0..31)
    for (int v = 0; v < 32; v++) {
        if ((v >= 8 && v <= 12) || v == 15) continue;
        set_gate(v, (uint64_t)exception_stubs + 16 * v, 0x8E);
    }

    // Системные вызовы: int 0x80 (DPL=3)
    set_gate(SYSCALL_VECTOR, (uint64_t)syscall_handler_asm, 0xEF);

    // Программное переключение задачи (yield): int 0x81
    set_gate(0x81, (uint64_t)sched_yield_asm, 0x8E);

    load_idt_ap();
}

void load_idt_ap(void) {
    struct IDTPointer idtr;
    idtr.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idtr.base  = (uint64_t)&idt;
    __asm__ volatile("lidt %0" :: "m"(idtr));
}
