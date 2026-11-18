#include "px.h"

int main(void) {
    PxMouse m;
    int was_hovered = -1;

    uint32_t btn_x = 100, btn_y = 900;
    uint32_t btn_w = 1720, btn_h = 75;

    for (;;) {
        int is_hovered = (m.x >= btn_x && m.x <= btn_x + btn_w &&
        m.y >= btn_y && m.y <= btn_y + btn_h);

        /* На первой итерации (was_hovered == -1) m еще не прочитан,
         * поэтому принудительно рисуем начальный кадр целиком. */
        if (was_hovered == -1 || is_hovered != was_hovered) {
            fillScreen(25, 25, 35, 255);
            if (was_hovered != -1 && is_hovered) {
                drawRect(btn_x, btn_y, btn_w, btn_h, 50, 190, 255, 255);
            } else {
                drawRect(btn_x, btn_y, btn_w, btn_h, 0, 140, 255, 255);
            }
            drawText("Exit", btn_x + 820, btn_y + 45, 255, 255, 255, 255);
            swapBuffers();
            was_hovered = (was_hovered == -1) ? 0 : is_hovered;
        }

        if (is_hovered && (m.buttons & MOUSE_LEFT)) {
            break;
        }

        /* Ждем следующего прерывания (движения мыши или тика таймера) */
        wait_mouse(&m);
    }

    sys_clear();
    sys_exit(0);
    return 0;
}
