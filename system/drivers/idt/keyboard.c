#include <stdint.h>
#include "../../kernel/kernel/kernel.h"

extern volatile int has_keyboard_event;
extern volatile uint8_t last_scancode;

void sys_reset_direct(void) {
    outb(0xCF9, 0x02);
    __asm__ volatile("outb %%al, $0x80" : : "a"(0)); // io_wait
    outb(0xCF9, 0x06);
    while(1) { __asm__ volatile("hlt"); }
}

volatile uint8_t kbd_ring_buffer[KBD_BUFFER_SIZE];
volatile int kbd_head = 0;
volatile int kbd_tail = 0;

void keyboard_handler_c() {
    uint8_t scancode = inb(0x60);

    if (scancode == 0x01) {
        sys_reset_direct();
    }

    // Кладем скан-код в кольцевой буфер
    int next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next_head != kbd_tail) { // Если буфер не переполнен, сохраняем
        kbd_ring_buffer[kbd_head] = scancode;
        kbd_head = next_head;
    }
    // Если переполнен — символ пропадёт, но ядро не заклинит

    send_apic_eoi(); // LAPIC свободен для новых прерываний!
}
