#include "qt.hpp"

extern "C" {
    void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha);
    void print(const char* text, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
}

Widget::Widget(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
    : x(x), y(y), w(w), h(h) {}

TerminalWindow::TerminalWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
    : Widget(x, y, w, h), cursor_x(20), cursor_y(50) {}

void TerminalWindow::write_text(const char* text) {
    // Выводим текст внутри окна терминала
    print(text, x + cursor_x, y + cursor_y, 0, 255, 0, 255); // Зеленый текст в стиле хакерского терминала
    cursor_y += 25; // Сдвиг строки вниз
}

void TerminalWindow::clear_terminal() {
    cursor_x = 20;
    cursor_y = 50;
    // Перерисовываем фон окна, очищая старый текст
    draw();
}

void TerminalWindow::draw() {
    // 1. Тело окна терминала (темно-серый полупрозрачный фон)
    draw_rect(x, y, w, h, 20, 20, 20, 240);

    // 2. Шапка окна (строка заголовка)
    draw_rect(x, y, w, 35, 50, 50, 50, 255);

    // 3. Заголовок окна
    print("AnalOS Terminal (Qt-Engine)", x + 15, y + 10, 220, 220, 220, 255);
}
