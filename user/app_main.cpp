#include "user_syscall.h"

// Простейшая функция отрисовки в User Space через полученный по сисколлу фреймбуфер
void draw_rect_user(uint32_t* fb, uint32_t screen_w, uint32_t screen_h,
                    uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                    uint8_t r, uint8_t g, uint8_t b) {
    if (x + w > screen_w) w = screen_w - x;
    if (y + h > screen_h) h = screen_h - y;

    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            uint32_t index = (y + i) * screen_w + (x + j);
            fb[index] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

extern "C" void program_main(void) {
    // 1. Запрашиваем фреймбуфер у ядра через системный вызов
    uint32_t* fb = sys_get_fb();
    if (!fb) {
        sys_exit(1);
    }

    uint32_t screen_w = 1920;
    uint32_t screen_h = 1080;

    // 2. Параметры окна терминала по центру экрана
    uint32_t term_w = 900;
    uint32_t term_h = 600;
    uint32_t term_x = (screen_w - term_w) / 2;
    uint32_t term_y = (screen_h - term_h) / 2;

    // 3. Рисуем тело терминала (темно-серый фон)
    draw_rect_user(fb, screen_w, screen_h, term_x, term_y, term_w, term_h, 30, 30, 30);

    // 4. Рисуем шапку окна (светло-серая полоса)
    draw_rect_user(fb, screen_w, screen_h, term_x, term_y, term_w, 35, 60, 60, 60);

    // 5. Даем команду ядру обновить экран через сисколл SYS_SWAP
    sys_swap();

    // 6. Завершаем работу программы
    sys_exit(0);
}
