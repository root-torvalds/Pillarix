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

void keyboard_handler_c() {
    uint8_t scancode = inb(0x60);

    if (scancode == 0x01) {
        sys_reset_direct();
    }

    last_scancode = scancode;
    has_keyboard_event = 1;

    send_apic_eoi(); // Вместо outb(0x20, 0x20);
}
