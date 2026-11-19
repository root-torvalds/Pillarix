#include "px.h" // Замените на имя вашего .h файла с syscall-ами, если оно другое
#include "wm.h"

static WindowManager wm;
static Window* win1;
static Window* win2;

// Сработает при нажатии на кнопку внутри окна About OS
void on_action_click(Window* self) {
    CALL(self, drawText, "Button clicked!", 20, 130, COLOR(0, 230, 118));
}

// Сработает при нажатии на кнопку Terminal на панели задач
void on_open_terminal(Window* self) {
    (void)self;
    win1->visible = true;
    wm_bring_to_front(&wm, win1);
}

int main(void) {
    wm_init(&wm);

    int32_t screen_w = 1920;
    int32_t bar_h    = 40;
    int32_t btn_size = 32;
    int32_t center_x = (screen_w - btn_size) / 2; // 944px — идеально по центру
    int32_t center_y = (bar_h - btn_size) / 2;    // 4px — отступ сверху и снизу

    Window* taskbar = addWindow(&wm, "", 0, 1080 - bar_h, screen_w, bar_h, WIN_FRAMELESS);
    CALL(taskbar, clear, COLOR(20, 20, 24));

    CALL(taskbar, addButton, "", center_x, center_y, btn_size, btn_size, COLOR(0, 120, 215), on_action_click);

    // 2. Окно терминала
    win1 = addWindow(&wm, "Terminal", 120, 100, 480, 320, WIN_DEFAULT);
    CALL(win1, clear, COLOR(18, 18, 22));
    CALL(win1, drawText, "root@pillarix:~# uname -a", 16, 26, COLOR(0, 230, 118));

    // 3. Окно About OS с интерактивной кнопкой
    win2 = addWindow(&wm, "About OS", 420, 240, 400, 250, WIN_DEFAULT);
    CALL(win2, drawText, "Pillarix Graphical Shell", 20, 30, COLOR(255, 255, 255));
    CALL(win2, addButton, "Click Me!", 20, 60, 140, 38, COLOR(0, 150, 136), on_action_click);

    // Первичная отрисовка при запуске
    wm_render(&wm);

    // Главный цикл (спит через wait_mouse, пока мышь не двигается)
    while (1) {
        wm_update(&wm);
        wm_render(&wm);
    }

    return 0;
}
