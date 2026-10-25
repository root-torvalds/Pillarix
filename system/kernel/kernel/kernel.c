#include "kernel.h"
#include "app_bin.h" // Скомпилированный бинарник пользовательской программы в виде массива байт

volatile int has_keyboard_event = 0;
volatile int has_mouse_event = 0;
volatile uint8_t last_scancode = 0;

extern int mouse_x;
extern int mouse_y;

// Внешние объявления подсистем памяти, PE-лоадера и перехода в Ring 3
#ifdef __cplusplus
extern "C" {
#endif
    uint64_t load_pe_executable(uint8_t* file_image);
    void jump_to_user_space(uint64_t entry_point, uint64_t user_stack);
    void* allocate_kernel_memory(uint32_t size);
    void* allocate_user_pages(uint64_t virtual_addr, uint64_t size);

    void __attribute__((ms_abi)) screen_switch_to_virtio(void);
    int  __attribute__((ms_abi)) init_virtio_gpu(void);
    int  __attribute__((ms_abi)) virtio_gpu_setup_screen(void);
#ifdef __cplusplus
}
#endif

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

void __attribute__((ms_abi)) kernel_main(BootInfo* info) {
    if (!info) {
        while(1) { __asm__ __volatile__("hlt"); }
    }

    init_screen_driver(info);
    fill_screen(0, 0, 0, 255);

    // Пропорциональное позиционирование иконки (центр экрана 1920x1080)
    draw_icon(1920 / 2 - 32, 1080 / 2 - 32);
    swap_buffers(0);

    init_idt();
    printf("[KERNEL] Initializing IDT...", 100, 200, 255, 255, 0, 255);
    swap_buffers(0);
    disable_pic();
    printf("[KERNEL] Disable PIC...", 100, 250, 255, 255, 0, 255);
    swap_buffers(0);
    init_local_apic();
    printf("[KERNEL] Initializing LAPIC...", 100, 300, 255, 255, 0, 255);
    swap_buffers(0);
    init_lapic_timer(1000);
    printf("[KERNEL] Initializing LAPIC timer...", 100, 350, 255, 255, 0, 255);
    swap_buffers(0);
    init_ioapic();
    printf("[KERNEL] Initializing I/O APIC...", 100, 400, 255, 255, 0, 255);
    swap_buffers(0);
    init_mouse();
    printf("[KERNEL] Initializing mouse...", 100, 450, 255, 255, 0, 255);
    swap_buffers(0);
    io_apic_set_redirect(1, 33);
    printf("[KERNEL] I/O APIC set redirect...", 100, 500, 255, 255, 0, 255);
    swap_buffers(0);
    init_intel_hda();
    printf("[KERNEL] Initializing Intel HDA...", 100, 550, 255, 255, 0, 255);
    swap_buffers(0);
    init_ahci();
    printf("[KERNEL] Initializing AHCI...", 100, 600, 255, 255, 0, 255);
    swap_buffers(0);
    init_ext4();
    printf("[KERNEL] Initializing Ext4...", 100, 650, 255, 255, 0, 255);
    swap_buffers(0);

    printf("[KERNEL] Initializing VirtIO GPU Transport...", 100, 700, 255, 255, 0, 255);
    int res = init_virtio_gpu();
    if (res != 0) {
        printf("FAILED!", 800, 200, 255, 0, 0, 255);
        swap_buffers(0);
        while(1) { __asm__ volatile("hlt"); }
    }
    printf("SUCCESS!", 800, 200, 0, 255, 0, 255);
    swap_buffers(0);

    int screen_res = virtio_gpu_setup_screen();

    if (screen_res == 0) {
        screen_switch_to_virtio();
        virtio_gpu_redraw();

        printf("[KERNEL] Initializing Hardware Cursor Plane...", 100, 760, 255, 255, 0, 255);
        swap_buffers(0);
        display_manager_init_hardware_cursor(os_framebuffer);

        // === ПОЛНОЦЕННЫЙ ЗАПУСК USER SPACE (RING 3) ===
        // Передаем массив байт образа программы в PE-лоадер
        uint64_t entry_point = load_pe_executable((uint8_t*)image_user_app);

        if (entry_point != 0) {
            // Выделяем пользовательский стек (64 КБ по виртуальному адресу 0x600000) с флагом User
            uint64_t stack_va = 0x5F0000;
            uint64_t stack_size = 0x10000;
            void* user_stack = allocate_user_pages(stack_va, stack_size);
            uint64_t user_stack_top = 0x600000;

            if (user_stack) {
                printf("[KERNEL] Launching User Space App in Ring 3...", 100, 810, 0, 255, 0, 255);
                swap_buffers(0);

                // Безопасный переход в Ring 3 через iretq
                jump_to_user_space(entry_point, user_stack_top);
            } else {
                printf("[KERNEL] Error: Failed to allocate user stack!", 100, 810, 255, 0, 0, 255);
                swap_buffers(0);
            }
        } else {
            printf("[KERNEL] Error: Invalid PE Header or Loading Failed!", 100, 810, 255, 0, 0, 255);
            swap_buffers(0);
        }

    } else {
        printf("[KERNEL] Display Setup Error!", 100, 500, 255, 0, 0, 255);
        swap_buffers(0);
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

    mouse_x = 1920 / 2;
    mouse_y = 1080 / 2;

    while (1) {
        __asm__ __volatile__("hlt");

        if (has_keyboard_event) {
            has_mouse_event = 0;
            has_keyboard_event = 0;
            if (last_scancode == 0x01) {
                sys_reset();
            }
        }
        else if (has_mouse_event) {
            has_mouse_event = 0;

            // Ограничение координат мыши 1920x1080
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= 1920) mouse_x = 1919;
            if (mouse_y >= 1080) mouse_y = 1079;

            if (mouse_x != last_sent_mouse_x || mouse_y != last_sent_mouse_y) {
                last_sent_mouse_x = mouse_x;
                last_sent_mouse_y = mouse_y;

                display_manager_move_cursor(mouse_x, mouse_y);
            }
        }
    }
}
