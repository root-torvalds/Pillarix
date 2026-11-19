#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_WINDOWS        16
#define MAX_DRAW_COMMANDS  64
#define MAX_BUTTONS        16
#define TITLE_BAR_HEIGHT   30

typedef struct {
    uint8_t r, g, b, a;
} PxColor;

#define COLOR(r, g, b)       ((PxColor){(r), (g), (b), 255})
#define COLORA(r, g, b, a)   ((PxColor){(r), (g), (b), (a)})

typedef enum {
    WIN_FRAMELESS = 0,
    WIN_DEFAULT   = 1
} WindowFlags;

typedef enum {
    CMD_RECT,
    CMD_ROUND_RECT,
    CMD_TEXT
} DrawCmdType;

typedef struct {
    DrawCmdType type;
    int32_t     x, y, w, h;
    uint32_t    rx, ry;
    PxColor     color;
    char        text[64];
} DrawCommand;

typedef struct Window Window;

typedef struct {
    bool    active;
    int32_t x, y, w, h;
    char    label[32];
    PxColor color;
    void    (*on_click)(Window* win);
} Button;

struct Window {
    bool        in_use;
    int32_t     x, y, w, h;
    uint32_t    flags;
    char        title[64];
    PxColor     bg_color;
    bool        visible;

    DrawCommand cmds[MAX_DRAW_COMMANDS];
    uint32_t    cmd_count;

    Button      buttons[MAX_BUTTONS];
    uint32_t    btn_count;

    void (*clear)(Window* self, PxColor color);
    void (*drawRect)(Window* self, int32_t rx, int32_t ry, int32_t rw, int32_t rh, PxColor color);
    void (*fillRoundedRect)(Window* self, int32_t rx, int32_t ry, int32_t rw, int32_t rh, uint32_t rad, PxColor color);
    void (*drawText)(Window* self, const char* text, int32_t rx, int32_t ry, PxColor color);
    void (*addButton)(Window* self, const char* label, int32_t rx, int32_t ry, int32_t rw, int32_t rh, PxColor color, void (*on_click)(Window*));
};

#define CALL(obj, method, ...) (obj)->method((obj), ##__VA_ARGS__)

typedef struct {
    Window   pool[MAX_WINDOWS];
    Window*  z_order[MAX_WINDOWS];
    uint32_t count;

    Window*  dragged_win;
    int32_t  drag_offset_x;
    int32_t  drag_offset_y;
    uint32_t prev_buttons;
    PxMouse  mouse;
} WindowManager;

// ============================================================================
// МЕТОДЫ ОКНА
// ============================================================================

static inline void _win_clear(Window* self, PxColor color) {
    if (!self) return;
    self->bg_color  = color;
    self->cmd_count = 0;
    self->btn_count = 0;
}

static inline void _win_draw_rect(Window* self, int32_t rx, int32_t ry, int32_t rw, int32_t rh, PxColor color) {
    if (!self || self->cmd_count >= MAX_DRAW_COMMANDS) return;
    DrawCommand* c = &self->cmds[self->cmd_count++];
    c->type  = CMD_RECT;
    c->x = rx; c->y = ry; c->w = rw; c->h = rh;
    c->color = color;
}

static inline void _win_fill_round_rect(Window* self, int32_t rx, int32_t ry, int32_t rw, int32_t rh, uint32_t rad, PxColor color) {
    if (!self || self->cmd_count >= MAX_DRAW_COMMANDS) return;
    DrawCommand* c = &self->cmds[self->cmd_count++];
    c->type  = CMD_ROUND_RECT;
    c->x = rx; c->y = ry; c->w = rw; c->h = rh;
    c->rx = rad; c->ry = rad;
    c->color = color;
}

static inline void _win_draw_text(Window* self, const char* text, int32_t rx, int32_t ry, PxColor color) {
    if (!self || !text || self->cmd_count >= MAX_DRAW_COMMANDS) return;
    DrawCommand* c = &self->cmds[self->cmd_count++];
    c->type  = CMD_TEXT;
    c->x = rx; c->y = ry;
    c->color = color;
    int i = 0;
    while (text[i] && i < 63) { c->text[i] = text[i]; i++; }
    c->text[i] = '\0';
}

static inline void _win_add_button(Window* self, const char* label, int32_t rx, int32_t ry, int32_t rw, int32_t rh, PxColor color, void (*on_click)(Window*)) {
    if (!self || self->btn_count >= MAX_BUTTONS) return;
    Button* b = &self->buttons[self->btn_count++];
    b->active   = true;
    b->x = rx; b->y = ry; b->w = rw; b->h = rh;
    b->color    = color;
    b->on_click = on_click;
    int i = 0;
    while (label && label[i] && i < 31) { b->label[i] = label[i]; i++; }
    b->label[i] = '\0';
}
// ============================================================================
// МЕНЕДЖЕР ОКОН (Часть 2: Инициализация, события мыши и рендер)
// ============================================================================

static inline void wm_init(WindowManager* wm) {
    wm->count        = 0;
    wm->dragged_win  = 0;
    wm->prev_buttons = 0;
    wm->mouse        = (PxMouse){0, 0, 0};
    for (int i = 0; i < MAX_WINDOWS; i++) wm->pool[i].in_use = false;
}

static inline Window* addWindow(WindowManager* wm, const char* title, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t flags) {
    if (!wm || wm->count >= MAX_WINDOWS) return 0;

    Window* win = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!wm->pool[i].in_use) { win = &wm->pool[i]; break; }
    }
    if (!win) return 0;

    win->in_use    = true;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->flags     = flags;
    win->visible   = true;
    win->bg_color  = COLOR(32, 32, 36);
    win->cmd_count = 0;
    win->btn_count = 0;

    int i = 0;
    while (title && title[i] && i < 63) { win->title[i] = title[i]; i++; }
    win->title[i] = '\0';

    win->clear           = _win_clear;
    win->drawRect        = _win_draw_rect;
    win->fillRoundedRect = _win_fill_round_rect;
    win->drawText        = _win_draw_text;
    win->addButton       = _win_add_button;

    wm->z_order[wm->count++] = win;
    return win;
}

static inline void wm_bring_to_front(WindowManager* wm, Window* target) {
    int idx = -1;
    for (uint32_t i = 0; i < wm->count; i++) {
        if (wm->z_order[i] == target) { idx = i; break; }
    }
    if (idx != -1 && idx < (int)(wm->count - 1)) {
        for (uint32_t i = idx; i < wm->count - 1; i++) wm->z_order[i] = wm->z_order[i + 1];
        wm->z_order[wm->count - 1] = target;
    }
}

// Обработка мыши: перетаскивание, клики по кнопкам и усыпление CPU при простое
static inline void wm_update(WindowManager* wm) {
    if (wm->dragged_win) {
        get_mouse(&wm->mouse);
    } else {
        wait_mouse(&wm->mouse);
    }

    int32_t mx = (int32_t)wm->mouse.x;
    int32_t my = (int32_t)wm->mouse.y;
    bool left_down    = (wm->mouse.buttons & MOUSE_LEFT) != 0;
    bool left_pressed = left_down && !(wm->prev_buttons & MOUSE_LEFT);

    if (left_pressed) {
        for (int i = (int)wm->count - 1; i >= 0; i--) {
            Window* w = wm->z_order[i];
            if (!w->visible) continue;

            if (mx >= w->x && mx <= w->x + w->w && my >= w->y && my <= w->y + w->h) {
                wm_bring_to_front(wm, w);

                int32_t title_h = (w->flags & WIN_DEFAULT) ? TITLE_BAR_HEIGHT : 0;

                // 1. Клик по шапке окна
                if (title_h > 0 && my <= w->y + title_h) {
                    if (mx >= w->x + w->w - 26) {
                        w->visible = false;
                    } else {
                        wm->dragged_win   = w;
                        wm->drag_offset_x = mx - w->x;
                        wm->drag_offset_y = my - w->y;
                    }
                    break;
                }

                // 2. Проверка клика по кнопкам внутри окна
                int32_t client_x = w->x;
                int32_t client_y = w->y + title_h;

                for (uint32_t b_idx = 0; b_idx < w->btn_count; b_idx++) {
                    Button* btn = &w->buttons[b_idx];
                    if (!btn->active) continue;

                    int32_t bx = client_x + btn->x;
                    int32_t by = client_y + btn->y;

                    if (mx >= bx && mx <= bx + btn->w && my >= by && my <= by + btn->h) {
                        if (btn->on_click) {
                            btn->on_click(w);
                        }
                        break;
                    }
                }
                break;
            }
        }
    }

    if (left_down && wm->dragged_win) {
        wm->dragged_win->x = mx - wm->drag_offset_x;
        int32_t new_y = my - wm->drag_offset_y;
        wm->dragged_win->y = (new_y < 0) ? 0 : new_y;
    } else {
        wm->dragged_win = 0;
    }

    wm->prev_buttons = wm->mouse.buttons;
}

// Отрисовка всего рабочего стола
static inline void wm_render(WindowManager* wm) {
    fillScreen(18, 82, 138, 255);

    int32_t mx = (int32_t)wm->mouse.x;
    int32_t my = (int32_t)wm->mouse.y;

    for (uint32_t i = 0; i < wm->count; i++) {
        Window* w = wm->z_order[i];
        if (!w || !w->visible) continue;

        int32_t title_h = (w->flags & WIN_DEFAULT) ? TITLE_BAR_HEIGHT : 0;

        // Клиппинг самого окна относительно левого (0) и верхнего (0) края экрана
        int32_t win_x = (w->x < 0) ? 0 : w->x;
        int32_t win_y = (w->y < 0) ? 0 : w->y;
        int32_t win_w = w->w - (win_x - w->x);
        int32_t win_h = w->h - (win_y - w->y);

        if (win_w > 0 && win_h > 0) {
            if (w->flags & WIN_DEFAULT) {
                int32_t bx = (w->x - 1 < 0) ? 0 : (w->x - 1);
                int32_t by = (w->y - 1 < 0) ? 0 : (w->y - 1);
                int32_t bw = (w->w + 2) - (bx - (w->x - 1));
                int32_t bh = (w->h + 2) - (by - (w->y - 1));

                fillRoundedRect(bx, by, bw, bh, 8, 8, QT_ABSOLUTE_SIZE, 65, 65, 72, 255);
                fillRoundedRect(win_x, win_y, win_w, win_h, 8, 8, QT_ABSOLUTE_SIZE, w->bg_color.r, w->bg_color.g, w->bg_color.b, 255);

                // Шапка окна
                int32_t th = TITLE_BAR_HEIGHT - (win_y - w->y);
                if (th > 0) {
                    fillRoundedRect(win_x, win_y, win_w, th, 8, 8, QT_ABSOLUTE_SIZE, 24, 24, 28, 255);
                }

                // Заголовок (рисуем, если левый край не уехал далеко за экран)
                if (w->x + 12 >= 0 && w->y + 20 >= 0) {
                    drawText(w->title, w->x + 12, w->y + 20, 230, 230, 235, 255);
                }

                // Кнопка закрытия
                if (w->x + w->w - 24 >= 0 && w->y + 6 >= 0) {
                    fillRoundedRect(w->x + w->w - 24, w->y + 6, 18, 18, 4, 4, QT_ABSOLUTE_SIZE, 232, 17, 35, 255);
                }
            } else {
                drawRect(win_x, win_y, win_w, win_h, w->bg_color.r, w->bg_color.g, w->bg_color.b, w->bg_color.a);
            }
        }

        int32_t client_x = w->x;
        int32_t client_y = w->y + title_h;
        int32_t client_w = w->w;
        int32_t client_h = w->h - title_h;

        // Отрисовка примитивов окна с защитой от ухода за левый и верхний край экрана
        for (uint32_t c_idx = 0; c_idx < w->cmd_count; c_idx++) {
            DrawCommand* cmd = &w->cmds[c_idx];

            if (cmd->type == CMD_TEXT) {
                int32_t tx = client_x + cmd->x;
                int32_t ty = client_y + cmd->y;
                if (cmd->x >= 0 && cmd->y >= 0 && cmd->y <= client_h && tx >= 0 && ty >= 0) {
                    drawText(cmd->text, tx, ty, cmd->color.r, cmd->color.g, cmd->color.b, cmd->color.a);
                }
                continue;
            }

            int32_t x1 = (cmd->x < 0) ? 0 : cmd->x;
            int32_t y1 = (cmd->y < 0) ? 0 : cmd->y;
            int32_t x2 = (cmd->x + cmd->w > client_w) ? client_w : (cmd->x + cmd->w);
            int32_t y2 = (cmd->y + cmd->h > client_h) ? client_h : (cmd->y + cmd->h);

            // Переводим в экранные координаты и обрезаем по левому/верхнему краю монитора
            int32_t sx1 = client_x + x1;
            int32_t sy1 = client_y + y1;
            int32_t sx2 = client_x + x2;
            int32_t sy2 = client_y + y2;

            if (sx1 < 0) sx1 = 0;
            if (sy1 < 0) sy1 = 0;
            if (sx1 >= sx2 || sy1 >= sy2) continue;

            if (cmd->type == CMD_RECT) {
                drawRect(sx1, sy1, sx2 - sx1, sy2 - sy1, cmd->color.r, cmd->color.g, cmd->color.b, cmd->color.a);
            } else if (cmd->type == CMD_ROUND_RECT) {
                fillRoundedRect(sx1, sy1, sx2 - sx1, sy2 - sy1, cmd->rx, cmd->ry, QT_ABSOLUTE_SIZE, cmd->color.r, cmd->color.g, cmd->color.b, cmd->color.a);
            }
        }

        // Отрисовка кнопок с клиппингом по левому/верхнему краю экрана
        for (uint32_t b_idx = 0; b_idx < w->btn_count; b_idx++) {
            Button* btn = &w->buttons[b_idx];
            if (!btn->active) continue;

            int32_t bx1 = client_x + btn->x;
            int32_t by1 = client_y + btn->y;
            int32_t bx2 = bx1 + btn->w;
            int32_t by2 = by1 + btn->h;

            bool hovered = (mx >= bx1 && mx <= bx2 && my >= by1 && my <= by2);

            if (bx1 < 0) bx1 = 0;
            if (by1 < 0) by1 = 0;
            if (bx1 >= bx2 || by1 >= by2) continue;

            uint8_t r = hovered ? (btn->color.r + 25 > 255 ? 255 : btn->color.r + 25) : btn->color.r;
            uint8_t g = hovered ? (btn->color.g + 25 > 255 ? 255 : btn->color.g + 25) : btn->color.g;
            uint8_t b = hovered ? (btn->color.b + 25 > 255 ? 255 : btn->color.b + 25) : btn->color.b;

            fillRoundedRect(bx1, by1, bx2 - bx1, by2 - by1, 6, 6, QT_ABSOLUTE_SIZE, r, g, b, 255);

            if (client_x + btn->x + 14 >= 0 && client_y + btn->y + (btn->h / 2) + 5 >= 0) {
                drawText(btn->label, client_x + btn->x + 14, client_y + btn->y + (btn->h / 2) + 5, 255, 255, 255, 255);
            }
        }
    }
    swapBuffers();
}
