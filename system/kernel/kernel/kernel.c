#include "kernel.h"

#define KBD_MOD_LSHIFT   (1 << 0)
#define KBD_MOD_RSHIFT   (1 << 1)
#define KBD_MOD_LCTRL    (1 << 2)
#define KBD_MOD_LALT     (1 << 3)
#define KBD_MOD_SHIFT    (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)

#define LINE_HEIGHT 20
#define HISTORY_MAX 16
#define CMD_MAX_LEN 512

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

static char cmd_history[HISTORY_MAX][CMD_MAX_LEN];
static int history_count = 0;
static int history_index = 0;
static char current_draft[CMD_MAX_LEN] = {0};

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

static void history_copy_str(char* dst, const char* src) {
    int k = 0;
    while (src[k] != '\0' && k < CMD_MAX_LEN - 1) {
        dst[k] = src[k];
        k++;
    }
    dst[k] = '\0';
}

static int history_str_len(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static int history_str_eq(const char* s1, const char* s2) {
    int k = 0;
    while (s1[k] != '\0' && s2[k] != '\0') {
        if (s1[k] != s2[k]) return 0;
        k++;
    }
    return (s1[k] == s2[k]);
}

static void history_add(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (history_count > 0 && history_str_eq(cmd_history[history_count - 1], cmd)) {
        history_index = history_count;
        return;
    }
    if (history_count < HISTORY_MAX) {
        history_copy_str(cmd_history[history_count], cmd);
        history_count++;
    } else {
        for (int k = 0; k < HISTORY_MAX - 1; k++) {
            history_copy_str(cmd_history[k], cmd_history[k + 1]);
        }
        history_copy_str(cmd_history[HISTORY_MAX - 1], cmd);
    }
    history_index = history_count;
}

#define EDIT_MAX 1024
static char edit_target[300];

static void make_abs_path(const char* in, char* out, int max) {
    int j = 0;
    if (in[0] != '/') out[j++] = '/';
    for (int k = 0; in[k] != '\0' && j < max - 1; k++) out[j++] = in[k];
    out[j] = '\0';
}

static const char* edit_err_text(int c) {
    if (c == -1)  return "parent dir not found";
    if (c == -2)  return "parent inode read failed";
    if (c == -3)  return "parent dir has no block";
    if (c == -4)  return "dir block read failed";
    if (c == -5)  return "file inode read failed";
    if (c == -6)  return "AHCI disk write failed";
    if (c == -7)  return "inode table write failed";
    if (c == -12) return "no space in directory block";
    if (c == -14) return "no free inode";
    if (c == -15) return "empty file name";
    if (c == -16) return "no free block";
    if (c == -17) return "file larger than 1024 bytes";
    return "unknown error";
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

extern char current_dir[];

static void redraw_input_line(int line_num, const char* cmd_str, int cmd_len, int cur_pos, int show_cursor) {
    int start_y = line_num * LINE_HEIGHT;

    for (int py = start_y - 15; py < start_y + 8; py++) {
        for (int px = 0; px < 800; px++) {
            drawPoint(px, py, 0, 0, 0, 255);
        }
    }
    int char_w = 10;
    int dir_len = history_str_len(current_dir);
    printf(current_dir, 0, start_y, 100, 200, 255, 255);
    printf(">", dir_len * char_w, start_y, 255, 255, 255, 255);

    int text_offset = (dir_len * char_w) + 12;

    for (int idx = 0; idx < cmd_len; idx++) {
        char str[2] = { cmd_str[idx], '\0' };
        printf(str, text_offset + (idx * 10), start_y, 255, 255, 255, 255);
    }

    if (show_cursor) {
        int cur_x = text_offset + (cur_pos * 10);
        printf("_", cur_x, start_y, 0, 255, 255, 255);
    }
}

static int rshift_down = 0;
int in_edit_mode = 0;
char edit_buffer[4096] = {0};
int edit_buf_len = 0;
extern char active_edit_filename[];

static void handle_keyboard_scancode(uint8_t current_scancode, char* command, int* i, int* cursor_pos, int* o, int* sh) {
    last_scancode = current_scancode;
    if (current_scancode == 0x2A) {
        *sh = 1;
    } else if (current_scancode == 0xAA) {
        *sh = 0;
    } else if (current_scancode == 0x36) {
        *sh = 1;
        rshift_down = 1;
    } else if (current_scancode == 0xB6) {
        *sh = 0;
        rshift_down = 0;
    } else if (current_scancode == 0x01) {
        // ESC
    } else if (in_edit_mode) {
        if (current_scancode == 0x1C) {
            if (*sh || rshift_down) {
                char dir_path[300];
                char file_name[256];
                ext4_split_path(edit_target, dir_path, file_name);

                int res;
                if (file_name[0] == '\0') res = -15;
                else res = ext4_create_file(dir_path, file_name, 0x8180, edit_buffer, edit_buf_len);

                char vbuf[64];
                for (int k = 0; k < 64; k++) vbuf[k] = 0;
                int vr = (res == 0) ? ext4_read_file_to_buffer(edit_target, vbuf, 63) : res;
                int expect = (edit_buf_len < 63) ? edit_buf_len : 63;

                in_edit_mode = 0;
                fillScreen(0, 0, 0, 255);
                *o = 1;
                int y0 = (*o) * LINE_HEIGHT;

                if (res == 0 && vr == expect) {
                    printf("File saved successfully.", 0, y0, 0, 255, 0, 255);
                } else if (res == 0) {
                    printf("Saved, but read-back mismatch! got:", 0, y0, 255, 80, 80, 255);
                    print_hex((uint32_t)vr, 400, y0);
                    printf("expected:", 0, y0 + LINE_HEIGHT, 255, 255, 0, 255);
                    print_hex((uint32_t)expect, 100, y0 + LINE_HEIGHT);
                } else {
                    printf("Error saving file, code:", 0, y0, 255, 80, 80, 255);
                    print_hex((uint32_t)res, 260, y0);
                    printf(edit_err_text(res), 0, y0 + LINE_HEIGHT, 255, 255, 0, 255);
                }
                printf("path:", 0, y0 + 2 * LINE_HEIGHT, 150, 150, 150, 255);
                printf(edit_target, 60, y0 + 2 * LINE_HEIGHT, 255, 255, 255, 255);
                (*o) += 4;
            } else {
                if (edit_buf_len < EDIT_MAX) {
                    edit_buffer[edit_buf_len++] = '\n';
                    edit_buffer[edit_buf_len] = '\0';
                }
            }
        } else if (current_scancode == 0x0E) {
            if (edit_buf_len > 0) {
                edit_buf_len--;
                edit_buffer[edit_buf_len] = '\0';
            }
        } else {
            char ascii = (current_scancode == 0x39) ? ' ' :
                         (*sh ? kbd_make_shift[current_scancode] : kbd_make_normal[current_scancode]);
            if (ascii != 0 && edit_buf_len < EDIT_MAX) {
                edit_buffer[edit_buf_len++] = ascii;
                edit_buffer[edit_buf_len] = '\0';
            }
        }
    } else {
        if (current_scancode == 0x53) {
            if (*cursor_pos < *i) {
                for (int k = *cursor_pos; k < *i - 1; k++) command[k] = command[k + 1];
                (*i)--;
                command[*i] = '\0';
            }
        } else if (current_scancode == 0x48) {
            if (history_count > 0 && history_index > 0) {
                if (history_index == history_count) {
                    command[*i] = '\0';
                    history_copy_str(current_draft, command);
                }
                history_index--;
                history_copy_str(command, cmd_history[history_index]);
                *i = history_str_len(command);
                *cursor_pos = *i;
            }
        } else if (current_scancode == 0x50) {
            if (history_index < history_count) {
                history_index++;
                if (history_index == history_count) {
                    history_copy_str(command, current_draft);
                } else {
                    history_copy_str(command, cmd_history[history_index]);
                }
                *i = history_str_len(command);
                *cursor_pos = *i;
            }
        } else if (current_scancode == 0x4B) {
            if (*cursor_pos > 0) (*cursor_pos)--;
        } else if (current_scancode == 0x4D) {
            if (*cursor_pos < *i) (*cursor_pos)++;
        } else if (current_scancode == 0x0E) {
            if (*cursor_pos > 0) {
                for (int k = *cursor_pos - 1; k < *i - 1; k++) command[k] = command[k + 1];
                (*i)--;
                (*cursor_pos)--;
                command[*i] = '\0';
            }
        } else if (current_scancode == 0x1C) {
            redraw_input_line(*o, command, *i, *cursor_pos, 0);

            if (*i > 0) {
                command[*i] = '\0';
                history_add(command);
                current_draft[0] = '\0';

                int result = terminal_execute_command(command, 0, *o, *o);

                if (result == -1) {
                    fillScreen(0, 0, 0, 255);
                    *o = 1;
                } else if (result == -2) {
                    if (active_edit_filename[0] == '\0') {
                        printf("edit: empty file name", 0, ((*o) + 1) * LINE_HEIGHT, 255, 80, 80, 255);
                        *o += 2;
                    } else {
                        make_abs_path(active_edit_filename, edit_target, sizeof(edit_target));
                        in_edit_mode = 1;
                        edit_buf_len = 0;
                        for (int f = 0; f < 4096; f++) edit_buffer[f] = 0;

                        int bytes_read = ext4_read_file_to_buffer(edit_target, edit_buffer, EDIT_MAX);
                        if (bytes_read > 0) {
                            edit_buf_len = bytes_read;
                            edit_buffer[edit_buf_len] = '\0';
                        }
                    }
                } else {
                    *o += result + 1;
                }
            } else {
                (*o)++;
            }

            *i = 0;
            *cursor_pos = 0;
            for (int f = 0; f < 512; f++) command[f] = 0;

            redraw_input_line(*o, command, *i, *cursor_pos, 1);
        } else {
            char ascii = (current_scancode == 0x39) ? ' ' :
                         (*sh ? kbd_make_shift[current_scancode] : kbd_make_normal[current_scancode]);
            if (ascii != 0 && *i < 510) {
                for (int k = *i; k > *cursor_pos; k--) command[k] = command[k - 1];
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
