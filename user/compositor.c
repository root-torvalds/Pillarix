#include "gfx.h"
#include "wm.h"

static void on_btn_close_desktop(Window* win) {
    if (win) win->visible = false;
}

int main(void) {
    gfx_init();

    WindowManager wm;
    wm_init(&wm);

    Window* info_win = addWindow(&wm, "Pillarix User-Space Compositor", 120, 100, 520, 260, WIN_DEFAULT);
    if (info_win) {
        CALL(info_win, clear, COLOR(30, 32, 38));
        CALL(info_win, drawText, "Rendering 100% in Ring 3 via SYS_MAP_FB", 20, 40, COLOR(100, 220, 255));
        CALL(info_win, drawText, "Zero syscalls per pixel/glyph!", 20, 70, COLOR(0, 255, 140));
        CALL(info_win, addButton, "Hide", 20, 140, 110, 36, COLOR(60, 120, 220), on_btn_close_desktop);
    }

    Window* taskbar = addWindow(&wm, "", 20, 1020, 1880, 46, WIN_FRAMELESS);
    if (taskbar) {
        CALL(taskbar, clear, COLORA(0, 0, 0, 0));
        CALL(taskbar, fillRoundedRect, 0, 0, 1880, 46, 12, COLORA(24, 24, 30, 220));
        CALL(taskbar, drawText, "Pillarix Compositor", 22, 29, COLOR(230, 230, 240));
    }

    for (;;) {
        if (taskbar) wm_bring_to_front(&wm, taskbar);
        wm_render(&wm);
        wm_update(&wm);
    }
    return 0;
}