#include "px.h"

int main(void) {
    fillScreen(25, 25, 35, 255);
    drawRect(100, 100, 200, 60, 0, 140, 255, 255);  /* кнопка выхода */
    swapBuffers();

    PxMouse m;
    for (;;) {
        wait_click(&m);
        if ((m.buttons & MOUSE_LEFT) &&
            m.x >= 100 && m.x < 300 &&
            m.y >= 100 && m.y < 160) {
            break;   /* кликнули по кнопке */
            }
    }
    sys_clear();
    return 0;
}
