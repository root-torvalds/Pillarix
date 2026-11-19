#include <stdint.h>
#include "../../kernel/kernel/kernel.h"

extern volatile int has_keyboard_event;
extern volatile uint8_t last_scancode;

void sys_reset_direct(void) {
    outb(0xCF9, 0x02);
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
    outb(0xCF9, 0x06);
    while(1) { __asm__ volatile("hlt"); }
}

volatile uint8_t kbd_ring_buffer[KBD_BUFFER_SIZE];
volatile int kbd_head = 0;
volatile int kbd_tail = 0;
spinlock_t kbd_lock = SPINLOCK_INIT;

void keyboard_handler_c(void) {
    uint64_t flags = spin_lock_irqsave(&kbd_lock);
    uint8_t scancode = inb(0x60);

    int next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next_head != kbd_tail) {
        kbd_ring_buffer[kbd_head] = scancode;
        kbd_head = next_head;
    }
    spin_unlock_irqrestore(&kbd_lock, flags);

    send_apic_eoi();
}
