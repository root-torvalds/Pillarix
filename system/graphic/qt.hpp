#pragma once
#include <stdint.h>

class Widget {
public:
    uint32_t x, y, w, h;
    Widget(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    virtual ~Widget() {}
    virtual void draw() = 0;
};

class TerminalWindow : public Widget {
private:
    uint32_t cursor_x;
    uint32_t cursor_y;
public:
    TerminalWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void write_text(const char* text);
    void clear_terminal();
    void draw() override;
};
