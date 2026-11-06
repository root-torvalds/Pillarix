#include "kernel.h"

#define KBD_MOD_LSHIFT   (1 << 0)
#define KBD_MOD_RSHIFT   (1 << 1)
#define KBD_MOD_LCTRL    (1 << 2)
#define KBD_MOD_LALT     (1 << 3)
#define KBD_MOD_SHIFT    (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)

typedef struct {
    uint8_t modifiers;
    bool caps_lock;
} kbd_state_t;

static kbd_state_t kbd;

volatile int has_keyboard_event = 0;
volatile int has_mouse_event = 0;
volatile uint8_t last_scancode = 0;

extern int mouse_x;
extern int mouse_y;

static const char kbd_break_normal[256] = {
    [0xA8] = '\'', [0x9E] = 'a',  [0xB0] = 'b',  [0xAE] = 'c',  [0xA0] = 'd',
    [0x92] = 'e',  [0xA1] = 'f',  [0xA2] = 'g',  [0xA3] = 'h',  [0x97] = 'i',
    [0xA4] = 'j',  [0xA5] = 'k',  [0xA6] = 'l',  [0xB2] = 'm',  [0xB1] = 'n',
    [0x98] = 'o',  [0x99] = 'p',  [0x90] = 'q',  [0x93] = 'r',  [0x9F] = 's',
    [0x94] = 't',  [0x96] = 'u',  [0xAF] = 'v',  [0x91] = 'w',  [0xAD] = 'x',
    [0x95] = 'y',  [0xAC] = 'z'
};

static const char kbd_break_shift[256] = {
    [0xA8] = '\"', [0x9E] = 'A',  [0xB0] = 'B',  [0xAE] = 'C',  [0xA0] = 'D',
    [0x92] = 'E',  [0xA1] = 'F',  [0xA2] = 'G',  [0xA3] = 'H',  [0x97] = 'I',
    [0xA4] = 'J',  [0xA5] = 'K',  [0xA6] = 'L',  [0xB2] = 'M',  [0xB1] = 'N',
    [0x98] = 'O',  [0x99] = 'P',  [0x90] = 'Q',  [0x93] = 'R',  [0x9F] = 'S',
    [0x94] = 'T',  [0x96] = 'U',  [0xAF] = 'V',  [0x91] = 'W',  [0xAD] = 'X',
    [0x95] = 'Y',  [0xAC] = 'Z'
};

static const char qm[8][8] = {
    {0,0,0,0,1,0,0,0},
    {0,0,0,0,1,0,0,0},
    {0,0,0,1,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0}
};

static const char qms[8][8] = {
    {0,0,0,1,0,0,1,0},
    {0,0,0,1,0,0,1,0},
    {0,0,1,0,0,1,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0}
};

static const char gt_matrix[8][8] = {
    {1, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0}
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

    init_idt();
    printf("[KERNEL] Initializing IDT...", 50, 150, 255, 255, 0, 255);
    swapBuffers(0);
    disable_pic();
    printf("[KERNEL] Disable PIC...", 50, 200, 255, 255, 0, 255);
    swapBuffers(0);
    init_local_apic();
    printf("[KERNEL] Initializing LAPIC...", 50, 250, 255, 255, 0, 255);
    swapBuffers(0);
    init_lapic_timer(1000);
    printf("[KERNEL] Initializing LAPIC timer...", 50, 300, 255, 255, 0, 255);
    swapBuffers(0);
    init_ioapic();
    printf("[KERNEL] Initializing I/O APIC...", 50, 350, 255, 255, 0, 255);
    swapBuffers(0);
    init_mouse();
    printf("[KERNEL] Initializing mouse...", 50, 400, 255, 255, 0, 255);
    swapBuffers(0);
    io_apic_set_redirect(1, 33);
    printf("[KERNEL] I/O APIC set redirect...", 50, 450, 255, 255, 0, 255);
    swapBuffers(0);
    init_intel_hda();
    printf("[KERNEL] Initializing Intel HDA...", 50, 500, 255, 255, 0, 255);
    swapBuffers(0);
    init_ahci();
    printf("[KERNEL] Initializing AHCI...", 50, 550, 255, 255, 0, 255);
    swapBuffers(0);
    init_ext4();
    printf("[KERNEL] Initializing Ext4...", 50, 600, 255, 255, 0, 255);
    swapBuffers(0);

    printf("[KERNEL] Initializing VirtIO GPU Transport...", 50, 650, 255, 255, 0, 255);
    int res = init_virtio_gpu();
    if (res != 0) {
        printf("FAILED!", 500, 100, 255, 0, 0, 255);
        swapBuffers(0);
        while(1) { __asm__ volatile("hlt"); }
    }
    printf("SUCCESS!", 500, 100, 0, 255, 0, 255);
    swapBuffers(0);


    printf("[KERNEL] Running virtio_gpu_setup_screen...", 50, 700, 255, 255, 0, 255);
    swapBuffers(0);

    int screen_res = virtio_gpu_setup_screen();
    printf("DONE!", 500, 130, 0, 255, 0, 255);
    swapBuffers(0);

    if (screen_res == 0) {

        screenSwitchToVirtio();


        printf("VirtIO GPU Display Setup: SUCCESS!", 50, 400, 0, 255, 0, 255);
        swapBuffers(0);


        virtio_gpu_redraw();


        printf("[KERNEL] Initializing Hardware Cursor Plane...", 50, 160, 255, 255, 0, 255);
        swapBuffers(0);
        display_manager_init_hardware_cursor(os_framebuffer);

    } else {

        printf("[KERNEL] Display Setup Error!", 50, 400, 255, 0, 0, 255);


        int qemu_resp = -screen_res;

        if (qemu_resp == 0x1200) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_UNSPEC (General Device Error)", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1201) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1202) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1203) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID (Conflict)", 50, 430, 255, 100, 100, 255);
        } else if (qemu_resp == 0x1205) {
            printf("QEMU RESP: VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER", 50, 430, 255, 100, 100, 255);
        } else {

            if (screen_res == -15) {
                printf("OS Error: Failed to allocate framebuffer RAM (Page aligned)", 50, 430, 255, 100, 100, 255);
            } else {
                printf("Unknown status code raw register value received", 50, 430, 255, 100, 100, 255);
            }
        }

        swapBuffers(0);
        while(1) { __asm__ volatile("hlt"); }
    }

    ext4_create_dir("/", "hello_world");
    ext4_create_file("/hello_world", "hello_world.txt", 0x1A4, "Hello World from Kernel!", 24);


    for (volatile int i = 0; i < 10000000; i++) {
        __asm__ volatile("nop");
    }


    __asm__ volatile("sti");

    static uint32_t last_sent_mouse_x = 9999;
    static uint32_t last_sent_mouse_y = 9999;

    mouse_x = 512;
    mouse_y = 384;

    fillScreen(0, 0, 0, 255);
    swapBuffers(0);

    char command[512];
    char null[256] = {0};
    int i = 0;
    int o = 1;
    int x = 0;

    {
        int gt_x = 0;
        int gt_y = (o * 15) - 8;
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (gt_matrix[row][col]) {
                    drawPoint(gt_x + col, gt_y + row, 255, 255, 255, 255);
                } else {
                    drawPoint(gt_x + col, gt_y + row, 0, 0, 0, 255);
                }
            }
        }
    }

    extern volatile uint8_t kbd_ring_buffer[];
    extern volatile int kbd_head;
    extern volatile int kbd_tail;

    while (1) {
        __asm__ __volatile__("hlt");
        static int sh = 0;
        int need_swap = 0;

        while (kbd_head != kbd_tail) {
            uint8_t current_scancode = kbd_ring_buffer[kbd_tail];
            kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;

            need_swap = 1;

            if (current_scancode == 0x2A || current_scancode == 0x36) {
                sh = 1;
            } else if (current_scancode == 0xAA || current_scancode == 0xB6) {
                sh = 0;
            } else if (current_scancode == 0x01) {
                sys_reset();
            } else if (current_scancode == 0x1C) {
                if (x > 0) {
                    command[x] = '\0';
                    int lines_used = terminal_execute_command(command, 0, o, o);
                    o = o + 1 + lines_used;
                } else {
                    o++;
                }
                i = 0;
                x = 0;
                for (int f = 0; f < 512; f++) command[f] = 0;

                {
                    int gt_x = 0;
                    int gt_y = (o * 15) - 8;
                    for (int row = 0; row < 8; row++) {
                        for (int col = 0; col < 8; col++) {
                            if (gt_matrix[row][col]) {
                                drawPoint(gt_x + col, gt_y + row, 255, 255, 255, 255);
                            } else {
                                drawPoint(gt_x + col, gt_y + row, 0, 0, 0, 255);
                            }
                        }
                    }
                }
            } else if (current_scancode == 0x39) { // Space
                if (i >= 128) { o++; i = 0; }
                printf(" ", i * 15, o * 15, 255, 255, 255, 255);
                command[x] = ' ';
                i++; x++;
            } else {
                char ascii = sh ? kbd_break_shift[current_scancode] : kbd_break_normal[current_scancode];
                if (ascii != 0) {
                    if (i >= 128) { o++; i = 0; }

                    int start_x = 15 + (i * 15);
                    int start_y = (o * 15) - 8;

                    if (ascii == 34 || ascii == '\'') {
                        const char (*matrix)[8] = (ascii == 34) ? qms : qm;

                        for (int row = 0; row < 8; row++) {
                            for (int col = 0; col < 8; col++) {
                                if (matrix[row][col]) {
                                    drawPoint(start_x + col, start_y + row, 255, 255, 255, 255);
                                } else {
                                    drawPoint(start_x + col, start_y + row, 0, 0, 0, 255);
                                }
                            }
                        }
                    } else {
                        char str[2] = { ascii, '\0' };
                        printf(str, start_x, o * 15, 255, 255, 255, 255);
                    }

                    command[x] = ascii;
                    i++; x++;
                }
            }
        } swapBuffers(0);
    }
}
