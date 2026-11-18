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
extern void mouse_handler_asm(void);
extern void keyboard_handler_asm(void);
extern void syscall_handler_asm(void);
extern uint8_t exception_stubs[];   /* 32 заглушек по 16 байт, interrupts.asm */

static void set_gate(int v, uint64_t handler, uint8_t attributes) {
    idt[v].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[v].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[v].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[v].segment_selector = KERNEL_CS;
    idt[v].ist = 0;
    idt[v].attributes = attributes;
    idt[v].reserved = 0;
}

void init_idt() {
    uint64_t dummy_address = (uint64_t)dummy_handler_asm;

    for(int i = 0; i < 256; i++) {
        idt[i].offset_low  = (uint16_t)(dummy_address & 0xFFFF);
        idt[i].offset_mid  = (uint16_t)((dummy_address >> 16) & 0xFFFF);
        idt[i].offset_high = (uint32_t)((dummy_address >> 32) & 0xFFFFFFFF);
        idt[i].segment_selector = KERNEL_CS;
        idt[i].ist = 0;
        idt[i].attributes = 0x8E;
        idt[i].reserved = 0;
    }

    uint64_t address = (uint64_t)keyboard_handler_asm;
    idt[33].offset_low  = (uint16_t)(address & 0xFFFF);
    idt[33].offset_mid  = (uint16_t)((address >> 16) & 0xFFFF);
    idt[33].offset_high = (uint32_t)((address >> 32) & 0xFFFFFFFF);
    idt[33].segment_selector = KERNEL_CS;
    idt[33].ist = 0;
    idt[33].attributes = 0x8E;
    idt[33].reserved = 0;

    uint64_t mouse_address = (uint64_t)mouse_handler_asm;
    idt[0x2C].offset_low  = (uint16_t)(mouse_address & 0xFFFF);
    idt[0x2C].offset_mid  = (uint16_t)((mouse_address >> 16) & 0xFFFF);
    idt[0x2C].offset_high = (uint32_t)((mouse_address >> 32) & 0xFFFFFFFF);
    idt[0x2C].segment_selector = KERNEL_CS;
    idt[0x2C].ist = 0;
    idt[0x2C].attributes = 0x8E;
    idt[0x2C].reserved = 0;

    // Исключения CPU: если упала пользовательская программа, ядро её убьёт
    // Векторы 8..12 и 15 не трогаем: до disable_pic() сюда может прийти IRQ0 8259
    // (вектор 8), и его должен молча съесть dummy_handler, как раньше.
    for (int v = 0; v < 32; v++) {
        if ((v >= 8 && v <= 12) || v == 15) continue;
        set_gate(v, (uint64_t)exception_stubs + 16 * v, 0x8E);
    }

    // Системные вызовы: int 0x80. 0xEF = trap gate с DPL=3 (разрешён из ring 3;
    // IF не сбрасывается, клавиатура и таймер работают во время вызова)
    set_gate(SYSCALL_VECTOR, (uint64_t)syscall_handler_asm, 0xEF);

    struct IDTPointer idtr;
    idtr.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idtr.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" :: "m"(idtr));
}
// Функция init_ioapic (которая была старым PIC) полностью удалена!
