#include "kernel.h"

#define KBD_MOD_LSHIFT   (1 << 0)
#define KBD_MOD_RSHIFT   (1 << 1)
#define KBD_MOD_LCTRL    (1 << 2)
#define KBD_MOD_LALT     (1 << 3)
#define KBD_MOD_SHIFT    (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)

#define LINE_HEIGHT 20

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
    [0x82] = '1',  [0x83] = '2',  [0x84] = '3',  [0x85] = '4',  [0x86] = '5',
    [0x87] = '6',  [0x88] = '7',  [0x89] = '8',  [0x8A] = '9',  [0x8B] = '0',
    [0x8C] = '-',  [0x8D] = '=',  [0xA9] = '`',  [0x9A] = '[',  [0x9B] = ']',
    [0xA7] = ';',  [0xA8] = '\'', [0xB3] = ',',  [0xB4] = '.',  [0xB5] = '/',
    [0xAB] = '\\', [0x9E] = 'a',  [0xB0] = 'b',  [0xAE] = 'c',  [0xA0] = 'd',
    [0x92] = 'e',  [0xA1] = 'f',  [0xA2] = 'g',  [0xA3] = 'h',  [0x97] = 'i',
    [0xA4] = 'j',  [0xA5] = 'k',  [0xA6] = 'l',  [0xB2] = 'm',  [0xB1] = 'n',
    [0x98] = 'o',  [0x99] = 'p',  [0x90] = 'q',  [0x93] = 'r',  [0x9F] = 's',
    [0x94] = 't',  [0x96] = 'u',  [0xAF] = 'v',  [0x91] = 'w',  [0xAD] = 'x',
    [0x95] = 'y',  [0xAC] = 'z'
};

static const char kbd_break_shift[256] = {
    [0x82] = '!',  [0x83] = '@',  [0x84] = '#',  [0x85] = '$',  [0x86] = '%',
    [0x87] = '^',  [0x88] = '&',  [0x89] = '*',  [0x8A] = '(',  [0x8B] = ')',
    [0x8C] = '_',  [0x8D] = '+',  [0xA9] = '~',  [0x9A] = '{',  [0x9B] = '}',
    [0xA7] = ':',  [0xA8] = '\"', [0xB3] = '<',  [0xB4] = '>',  [0xB5] = '?',
    [0xAB] = '|',  [0x9E] = 'A',  [0xB0] = 'B',  [0xAE] = 'C',  [0xA0] = 'D',
    [0x92] = 'E',  [0xA1] = 'F',  [0xA2] = 'G',  [0xA3] = 'H',  [0x97] = 'I',
    [0xA4] = 'J',  [0xA5] = 'K',  [0xA6] = 'L',  [0xB2] = 'M',  [0xB1] = 'N',
    [0x98] = 'O',  [0x99] = 'P',  [0x90] = 'Q',  [0x93] = 'R',  [0x9F] = 'S',
    [0x94] = 'T',  [0x96] = 'U',  [0xAF] = 'V',  [0x91] = 'W',  [0xAD] = 'X',
    [0x95] = 'Y',  [0xAC] = 'Z'
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

void init_kernel() {
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
}

static void redraw_input_line(int line_num, const char* cmd_str, int cmd_len, int cur_pos, int show_cursor) {
    int start_y = line_num * LINE_HEIGHT;

    // 1. Очищаем область строки черным цветом
    for (int py = start_y - 15; py < start_y + 8; py++) {
        for (int px = 0; px < 800; px++) {
            drawPoint(px, py, 0, 0, 0, 255);
        }
    }

    // 2. Рисуем приглашение
    printf(">", 0, start_y, 255, 255, 255, 255);

    // 3. Рисуем введенный текст символ за символом
    for (int idx = 0; idx < cmd_len; idx++) {
        char str[2] = { cmd_str[idx], '\0' };
        printf(str, 15 + (idx * 10), start_y, 255, 255, 255, 255);
    }

    // 4. Рисуем мигающий курсор
    if (show_cursor) {
        int cur_x = 15 + (cur_pos * 10);
        printf("_", cur_x, start_y, 0, 255, 255, 255);
    }
}

static int rshift_down = 0;
int in_edit_mode = 0;
char edit_buffer[4096] = {0};
int edit_buf_len = 0;
extern char active_edit_filename[];

// Сервис обработки сканкодов клавиатуры, шелла и редактора файлов
static void handle_keyboard_scancode(uint8_t current_scancode, char* command, int* i, int* cursor_pos, int* o, int* sh) {
    // Отслеживание нажатий/отпусканий Shift (левый и правый)
    if (current_scancode == 0x2A) {
        *sh = 1; // Left Shift press
    } else if (current_scancode == 0xAA) {
        *sh = 0; // Left Shift release
    } else if (current_scancode == 0x36) {
        *sh = 1;
        rshift_down = 1; // Right Shift press
    } else if (current_scancode == 0xB6) {
        *sh = 0;
        rshift_down = 0; // Right Shift release
    } else if (current_scancode == 0x01) {
        // ESC
    } else if (in_edit_mode) {
        // --- РЕЖИМ РЕДАКТИРОВАНИЯ ФАЙЛА (edit) ---
        if (current_scancode == 0x1C) { // Enter
            if (rshift_down) {
                // RShift + Enter: Сохранение файла и выход в терминал
                ext4_create_file("/", active_edit_filename, 0x8180, edit_buffer, edit_buf_len);
                in_edit_mode = 0;
                fillScreen(0, 0, 0, 255);
                *o = 1;
                printf("File saved successfully.", 0, (*o) * LINE_HEIGHT, 0, 255, 0, 255);
                (*o) += 2;
            } else {
                // Обычный Enter — перевод строки внутри файла
                if (edit_buf_len < 4095) {
                    edit_buffer[edit_buf_len++] = '\n';
                }
            }
        } else if (current_scancode == 0x0E) { // Backspace
            if (edit_buf_len > 0) {
                edit_buf_len--;
                edit_buffer[edit_buf_len] = '\0';
            }
        } else {
            char ascii = 0;
            if (current_scancode == 0x39) {
                ascii = ' ';
            } else {
                ascii = *sh ? kbd_break_shift[current_scancode] : kbd_break_normal[current_scancode];
            }

            if (ascii != 0 && edit_buf_len < 4095) {
                edit_buffer[edit_buf_len++] = ascii;
                edit_buffer[edit_buf_len] = '\0';
            }
        }
    } else {
        // --- ОБЫЧНЫЙ РЕЖИМ КОМАНДНОЙ СТРОКИ ---
        if (current_scancode == 0x53) { // Delete
            if (*cursor_pos < *i) {
                for (int k = *cursor_pos; k < *i - 1; k++) {
                    command[k] = command[k + 1];
                }
                (*i)--;
                command[*i] = '\0';
            }
        } else if (current_scancode == 0x4B) { // Стрелка влево
            if (*cursor_pos > 0) {
                (*cursor_pos)--;
            }
        } else if (current_scancode == 0x4D) { // Стрелка вправо
            if (*cursor_pos < *i) {
                (*cursor_pos)++;
            }
        } else if (current_scancode == 0x0E) { // Backspace
            if (*cursor_pos > 0) {
                for (int k = *cursor_pos - 1; k < *i - 1; k++) {
                    command[k] = command[k + 1];
                }
                (*i)--;
                (*cursor_pos)--;
                command[*i] = '\0';
            }
        } else if (current_scancode == 0x1C) { // Enter
            redraw_input_line(*o, command, *i, *cursor_pos, 0);

            if (*i > 0) {
                command[*i] = '\0';
                int result = terminal_execute_command(command, 0, *o, *o);

                if (result == -1) { // clear
                    fillScreen(0, 0, 0, 255);
                    *o = 1;
                } else if (result == -2) { // Запуск режима редактора (команда edit)
                    in_edit_mode = 1;
                    edit_buf_len = 0;
                    for (int f = 0; f < 4096; f++) edit_buffer[f] = 0;
                } else {
                    *o += result + 1;
                }
            } else {
                (*o)++;
            }

            *i = 0;
            *cursor_pos = 0;
            for (int f = 0; f < 512; f++) command[f] = 0;
        } else {
            char ascii = 0;
            if (current_scancode == 0x39) {
                ascii = ' ';
            } else {
                ascii = *sh ? kbd_break_shift[current_scancode] : kbd_break_normal[current_scancode];
            }

            if (ascii != 0 && *i < 510) {
                for (int k = *i; k > *cursor_pos; k--) {
                    command[k] = command[k - 1];
                }
                command[*cursor_pos] = ascii;
                (*i)++;
                (*cursor_pos)++;
                command[*i] = '\0';
            }
        }
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
    init_kernel();

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

    fillScreen(0, 0, 0, 255);
    swapBuffers(0);

    char command[512] = {0};
    int i = 0;          // Длина введенной строки
    int o = 1;          // Номер текущей строки консоли
    int cursor_pos = 0; // Позиция курсора
    int sh = 0;

    uint32_t cursor_ticks = 0;
    int cursor_visible = 1;

    // Первоначальный вывод приглашения и курсора
    redraw_input_line(o, command, i, cursor_pos, cursor_visible);

    extern volatile uint8_t kbd_ring_buffer[];
    extern volatile int kbd_head;
    extern volatile int kbd_tail;

    // Внешние переменные редактора (объявлены выше или здесь)
    extern int in_edit_mode;
    extern char edit_buffer[];
    extern int edit_buf_len;
    extern char active_edit_filename[];

    while (1) {
        __asm__ __volatile__("hlt");
        int need_redraw = 0;

        // --- Мигание курсора (только в обычном режиме терминала) ---
        if (!in_edit_mode) {
            cursor_ticks++;
            if (cursor_ticks % 10 == 0) {
                cursor_visible = !cursor_visible;
                need_redraw = 1;
            }
        }

        // --- Обработка событий клавиатуры из кольцевого буфера ---
        while (kbd_head != kbd_tail) {
            uint8_t current_scancode = kbd_ring_buffer[kbd_tail];
            kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;

            handle_keyboard_scancode(current_scancode, command, &i, &cursor_pos, &o, &sh);

            need_redraw = 1;
            cursor_visible = 1; // При нажатии курсор всегда виден
            cursor_ticks = 0;   // Сброс таймера мигания
        }

        // --- Перерисовка экрана при изменениях ---
        if (need_redraw) {
            if (in_edit_mode) {
                // Рендеринг содержимого файла в режиме редактора
                fillScreen(0, 0, 0, 255);
                printf("--- EDITING FILE (Press RShift+Enter to save & exit) ---", 0, 20, 200, 200, 0, 255);

                int cur_x = 0;
                int cur_y = 50;
                for (int idx = 0; idx < edit_buf_len; idx++) {
                    if (edit_buffer[idx] == '\n') {
                        cur_x = 0;
                        cur_y += LINE_HEIGHT;
                    } else {
                        char s[2] = { edit_buffer[idx], '\0' };
                        printf(s, cur_x, cur_y, 255, 255, 255, 255);
                        cur_x += 10;
                        if (cur_x > 780) {
                            cur_x = 0;
                            cur_y += LINE_HEIGHT;
                        }
                    }
                }
            } else {
                redraw_input_line(o, command, i, cursor_pos, cursor_visible);
            }
        } swapBuffers(0);
    }
}
