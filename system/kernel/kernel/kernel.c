#include "kernel.h"

#define KBD_MOD_LSHIFT   (1 << 0)
#define KBD_MOD_RSHIFT   (1 << 1)
#define KBD_MOD_LCTRL    (1 << 2)
#define KBD_MOD_LALT     (1 << 3)
#define KBD_MOD_SHIFT    (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)

volatile int has_keyboard_event = 0;
volatile int has_mouse_event = 0;
volatile uint8_t last_scancode = 0;

extern int mouse_x;
extern int mouse_y;

static const char kbd_make_normal[256] = {
    [0x02] = '1',  [0x03] = '2',  [0x04] = '3',  [0x05] = '4',  [0x06] = '5',
    [0x07] = '6',  [0x08] = '7',  [0x09] = '8',  [0x0A] = '9',  [0x0B] = '0',
    [0x0C] = '-',  [0x0D] = '=',  [0x29] = '`',  [0x1A] = '[',  [0x1B] = ']',
    [0x27] = ';',  [0x28] = '\'', [0x33] = ',',  [0x34] = '.',  [0x35] = '/',
    [0x2B] = '\\', [0x1E] = 'a',  [0x30] = 'b',  [0x2E] = 'c',  [0x20] = 'd',
    [0x12] = 'e',  [0x21] = 'f',  [0x22] = 'g',  [0x23] = 'h',  [0x17] = 'i',
    [0x24] = 'j',  [0x25] = 'k',  [0x26] = 'l',  [0x32] = 'm',  [0x31] = 'n',
    [0x18] = 'o',  [0x19] = 'p',  [0x10] = 'q',  [0x13] = 'r',  [0x1F] = 's',
    [0x14] = 't',  [0x16] = 'u',  [0x2F] = 'v',  [0x11] = 'w',  [0x2D] = 'x',
    [0x15] = 'y',  [0x2C] = 'z'
};

static const char kbd_make_shift[256] = {
    [0x02] = '!',  [0x03] = '@',  [0x04] = '#',  [0x05] = '$',  [0x06] = '%',
    [0x07] = '^',  [0x08] = '&',  [0x09] = '*',  [0x0A] = '(',  [0x0B] = ')',
    [0x0C] = '_',  [0x0D] = '+',  [0x29] = '~',  [0x1A] = '{',  [0x1B] = '}',
    [0x27] = ':',  [0x28] = '\"', [0x33] = '<',  [0x34] = '>',  [0x35] = '?',
    [0x2B] = '|',  [0x1E] = 'A',  [0x30] = 'B',  [0x2E] = 'C',  [0x20] = 'D',
    [0x12] = 'E',  [0x21] = 'F',  [0x22] = 'G',  [0x23] = 'H',  [0x17] = 'I',
    [0x24] = 'J',  [0x25] = 'K',  [0x26] = 'L',  [0x32] = 'M',  [0x31] = 'N',
    [0x18] = 'O',  [0x19] = 'P',  [0x10] = 'Q',  [0x13] = 'R',  [0x1F] = 'S',
    [0x14] = 'T',  [0x16] = 'U',  [0x2F] = 'V',  [0x11] = 'W',  [0x2D] = 'X',
    [0x15] = 'Y',  [0x2C] = 'Z'
};

void sys_reset(void) {
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    while(1) { __asm__ volatile("hlt"); }
}

void sys_shutdown(void) {
    outw(0xB2, 0x07);
    io_wait();
    outw(0x604, 0x2000);
    io_wait();

    outl(0xCF8, 0x8000F840);
    io_wait();
    uint32_t pmbase = inl(0xCFC) & 0xFFFE;
    io_wait();

    if (pmbase != 0) {
        uint16_t pm1_cnt_port = (uint16_t)(pmbase + 0x04);
        outw(pm1_cnt_port, 0x2000 | 0x1C00);
        io_wait();
    }

    __asm__ __volatile__("cli; hlt");
}

void init_kernel(BootInfo* info) {
    init_gdt();
    printf("[KERNEL] Initializing GDT...", 50, 150, 255, 255, 0, 255);
    swapBuffers(0);
    init_idt();
    printf("[KERNEL] Initializing IDT...", 50, 180, 255, 255, 0, 255);
    swapBuffers(0);
    disable_pic();
    printf("[KERNEL] Disable PIC...", 50, 210, 255, 255, 0, 255);
    swapBuffers(0);
    init_local_apic();
    printf("[KERNEL] Initializing LAPIC...", 50, 240, 255, 255, 0, 255);
    swapBuffers(0);
    init_apic(info);
    printf("[KERNEL] Parsing ACPI MADT...", 50, 270, 255, 255, 0, 255);
    swapBuffers(0);
    init_scheduler();
    printf("[KERNEL] Initializing Scheduler...", 50, 300, 255, 255, 0, 255);
    swapBuffers(0);
    init_smp();
    printf("[KERNEL] Starting SMP cores...", 50, 330, 255, 255, 0, 255);
    swapBuffers(0);
    init_lapic_timer(10000000);
    printf("[KERNEL] Initializing LAPIC timer...", 50, 360, 255, 255, 0, 255);
    swapBuffers(0);
    init_ioapic();
    printf("[KERNEL] Initializing I/O APIC...", 50, 390, 255, 255, 0, 255);
    swapBuffers(0);
    init_mouse();
    printf("[KERNEL] Initializing mouse...", 50, 420, 255, 255, 0, 255);
    swapBuffers(0);
    io_apic_set_redirect(1, 33);
    init_intel_hda();
    printf("[KERNEL] Initializing Intel HDA...", 50, 450, 255, 255, 0, 255);
    swapBuffers(0);
    init_ahci();
    printf("[KERNEL] Initializing AHCI...", 50, 480, 255, 255, 0, 255);
    swapBuffers(0);
}

int kbd_wait_char(void) {
    extern volatile uint8_t kbd_ring_buffer[];
    extern volatile int kbd_head;
    extern volatile int kbd_tail;
    extern spinlock_t kbd_lock;
    static int shift = 0;

    for (;;) {
        uint64_t flags = spin_lock_irqsave(&kbd_lock);
        if (kbd_head == kbd_tail) {
            spin_unlock_irqrestore(&kbd_lock, flags);
            __asm__ volatile("sti; hlt");
            continue;
        }
        uint8_t sc = kbd_ring_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
        spin_unlock_irqrestore(&kbd_lock, flags);

        if (sc == 0x2A || sc == 0x36) { shift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift = 0; continue; }
        if (sc == 0x1C) return '\n';
        if (sc == 0x0E) return '\b';
        if (sc == 0x39) return ' ';

        char c = shift ? kbd_make_shift[sc] : kbd_make_normal[sc];
        if (c != 0) return (int)(unsigned char)c;
    }
}

#ifdef __cplusplus
extern "C" {
#endif
void __attribute__((ms_abi)) screen_switch_to_virtio(void);
int  __attribute__((ms_abi)) init_virtio_gpu(void);
int  __attribute__((ms_abi)) virtio_gpu_setup_screen(void);
#ifdef __cplusplus
}
#endif

void __attribute__((ms_abi)) kernel_main(BootInfo* info) {
    if (!info) {
        while(1) { __asm__ __volatile__("hlt"); }
    }

    initScreenDriver(info);
    fillScreen(0, 0, 0, 255);
    swapBuffers(0);
    init_kernel(info);

    printf("[KERNEL] Initializing VirtIO GPU Transport...", 50, 650, 255, 255, 0, 255);
    int res = init_virtio_gpu();
    if (res != 0) {
        printf("FAILED!", 500, 100, 255, 0, 0, 255);
        swapBuffers(0);
        while(1) { __asm__ volatile("hlt"); }
    }
    printf("SUCCESS!", 500, 100, 0, 255, 0, 255);
    swapBuffers(0);

    int screen_res = virtio_gpu_setup_screen();
    if (screen_res == 0) {
        screenSwitchToVirtio();
        virtio_gpu_redraw();
        display_manager_init_hardware_cursor(os_framebuffer);
    } else {
        printf("[KERNEL] Display Setup Error!", 50, 400, 255, 0, 0, 255);
        swapBuffers(0);
        while(1) { __asm__ volatile("hlt"); }
    }

    for (volatile int i = 0; i < 10000000; i++) {
        __asm__ volatile("nop");
    }
    __asm__ volatile("sti");

    fillScreen(0, 0, 0, 255);
    printf("SMP Cores online:", 0, 100, 0, 255, 100, 255);
    print_hex((uint32_t)smp_cpu_count(), 200, 100);
    swapBuffers(0);

    const char* shell_paths[] = {
        "/apps/core/sh",
        "/apps/core/sh.plx",
        "/sh.plx",
        "/sh"
    };

    while (1) {
        int tid = -1;
        int y = 140;
        for (int p = 0; p < 4; p++) {
            printf("[KERNEL] Trying:", 15, y, 200, 200, 200, 255);
            printf(shell_paths[p], 190, y, 100, 200, 255, 255);
            swapBuffers(0);

            tid = px_spawn(shell_paths[p]);
            if (tid >= 0) break;

            printf("err:", 420, y, 255, 80, 80, 255);
            print_hex((uint32_t)tid, 470, y);
            swapBuffers(0);
            y += 20;
        }

        if (tid >= 0) {
            uint64_t code = px_wait(tid);
            fillScreen(0, 0, 0, 255);
            printf("[KERNEL] Shell exited, code:", 15, 20, 255, 255, 0, 255);
            print_hex((uint32_t)code, 310, 20);
            swapBuffers(0);
        } else {
            printf("Failed to start shell (-3=not found, -4=bad PLX, -5=vm_map)", 15, y + 20, 255, 80, 80, 255);
            swapBuffers(0);
            while (1) { __asm__ volatile("hlt"); }
        }
    }
}
