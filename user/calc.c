#include "px.h"

int main(void) {
    PxMouse m = {0, 0, 0};
    int was_hov = -1;

    char title[] = {'P','i','l','l','a','r','i','x',' ','T','e','s','t',0};
    char btn[]   = {'E','x','i','t',0};

    for (;;) {
        int hov = (m.x >= 760 && m.x <= 1160 && m.y >= 940 && m.y <= 1010);

        if (was_hov == -1 || hov != was_hov) {
            fillScreen(20, 22, 30, 255);

            /* 1. Текст по центру */
            drawText(title, (1920 - measureText(title)) / 2, 80, 255, 255, 255, 255);

            /* 2. Сглаженная линия */
            drawLine(300, 300, 700, 700, 80, 220, 255, 255);

            /* 3. Круг */
            drawEllipse(1100, 350, 300, 300, 255, 120, 180, 255);

            /* 4. Кнопка с центрированным текстом */
            drawRect(760, 940, 400, 70, hov ? 50 : 0, hov ? 190 : 140, 255, 255);
            drawText(btn, 760 + (400 - measureText(btn)) / 2, 983, 255, 255, 255, 255);

            swapBuffers();
            was_hov = (was_hov == -1) ? 0 : hov;
        }

        if (hov && (m.buttons & MOUSE_LEFT)) break;
        wait_mouse(&m);
    }

    sys_clear();
    sys_exit(0);
    return 0;
}
