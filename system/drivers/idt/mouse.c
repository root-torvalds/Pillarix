#include <stdint.h>
#include "../../kernel/kernel/kernel.h"
extern volatile int has_keyboard_event;
extern volatile int has_mouse_event;
extern volatile uint8_t last_scancode;

int mouse_last_x = 512;
int mouse_last_y = 384;
int mouse_cycle = 0;
int mouse_x = 512;
int mouse_y = 384;

uint8_t mouse_bytes[4];

void mouse_wait_write() {
    volatile uint32_t timeout = 1000000;
    while ((inb(0x64) & 2) != 0 && --timeout) {
        io_wait();
    }
}

void mouse_wait_read() {
    volatile uint32_t timeout = 1000000;
    while ((inb(0x64) & 1) == 0 && --timeout) {
        io_wait();
    }
}

void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, data);
}

uint8_t mouse_read() {
    mouse_wait_read();
    return inb(0x60);
}

void init_mouse() {
    uint8_t init_status = inb(0x64);
    if (init_status == 0xFF) {
        return;
    }

    volatile uint32_t clean = 1000;
    while ((inb(0x64) & 1) && --clean) {
        inb(0x60);
        io_wait();
    }

    mouse_wait_write();
    outb(0x64, 0xA8);

    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t comp_status = inb(0x60);

    comp_status |= 0x02;
    comp_status &= ~0x20;

    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, comp_status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    mouse_cycle = 0;
    mouse_x = 512;
    mouse_y = 384;
}

void mouse_handler_c() {
    uint8_t status = inb(0x64);

    if ((status & 0x01) == 0 || (status & 0x20) == 0) {
        send_apic_eoi(); // Вместо отправки на старые порты PIC
        return;
    }

    uint8_t data = inb(0x60);

    if (mouse_cycle == 0 && (data & 0x08) == 0) {
        mouse_cycle = 0;
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    mouse_bytes[mouse_cycle] = data;
    mouse_cycle++;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        int32_t move_x = (int8_t)mouse_bytes[1];
        int32_t move_y = (int8_t)mouse_bytes[2];

        mouse_x += move_x;
        mouse_y -= move_y;

        if (mouse_x < 0)    mouse_x = 0;
        if (mouse_y < 0)    mouse_y = 0;
        if (mouse_x > 1919) mouse_x = 1919;
        if (mouse_y > 1079)  mouse_y = 1079;

        has_mouse_event = 1;

        mouse_last_x = mouse_x;
        mouse_last_y = mouse_y;
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
    send_apic_eoi();
}
