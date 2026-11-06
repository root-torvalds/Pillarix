#include "qt.h"
#include "../kernel/kernel/kernel.h"

#ifdef __cplusplus
extern "C"
#endif
uint32_t *os_framebuffer;
extern uint32_t* real_framebuffer;
extern EFI_GRAPHICS_OUTPUT_BLT_PIXEL* virtual_framebuffer;

void initScreenDriver(BootInfo* info) {
    if (info) {
        real_framebuffer = (uint32_t*)(*info).FrameBufferBase;
        virtual_framebuffer = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)(*info).VirtualFrameBuffer;
    }
}

void screenSwitchToVirtio(void) {
    if (os_framebuffer != 0) {
        virtual_framebuffer = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)os_framebuffer;
    }
}

EFIAPI EFI_GRAPHICS_OUTPUT_BLT_PIXEL getPixel(uint32_t x, uint32_t y) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL empty_pixel = {0, 0, 0, 0};

    if (!virtual_framebuffer || x >= 1920 || y >= 1080) {
        return empty_pixel;
    }

    uint32_t index = y * 1920 + x;
    return virtual_framebuffer[index];
}
EFIAPI void swapBuffers(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    (void)gop;
    if (!real_framebuffer || !virtual_framebuffer) return;

    UINT32* src = (UINT32*)virtual_framebuffer;
    for (UINT32 i = 0; i < 1920 * 1080; i++) {
        UINT32 color = src[i];
        UINT8 r = (color >> 16) & 0xFF;
        UINT8 g = (color >> 8) & 0xFF;
        UINT8 b = color & 0xFF;

        real_framebuffer[i] = (r << 16) | (g << 8) | b;
    }
}

EFIAPI void drawPoint(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!virtual_framebuffer || x >= 1920 || y >= 1080 || a == 0) return;

    uint32_t index = y * 1920 + x;
    if (a == 255) {
        virtual_framebuffer[index].Red   = r;
        virtual_framebuffer[index].Green = g;
        virtual_framebuffer[index].Blue  = b;
        return;
    }

    uint8_t bg_r = virtual_framebuffer[index].Red;
    uint8_t bg_g = virtual_framebuffer[index].Green;
    uint8_t bg_b = virtual_framebuffer[index].Blue;

    virtual_framebuffer[index].Red   = ((r * a) + (bg_r * (255 - a))) >> 8;
    virtual_framebuffer[index].Green = ((g * a) + (bg_g * (255 - a))) >> 8;
    virtual_framebuffer[index].Blue  = ((b * a) + (bg_b * (255 - a))) >> 8;
}

// Основная функция отрисовки линии (Алгоритм Брезенхема)
void drawLine(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Ранний возврат, если линия полностью прозрачна
    if (a == 0) return;

    // Вычисляем абсолютные разницы координат
    int dx = (x2 >= x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 >= y1) ? (y2 - y1) : (y1 - y2);

    // Определяем направление движения по осям (1 или -1)
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;

    int err = dx - dy;

    while (true) {
        // Рисуем текущую точку
        drawPoint(x1, y1, r, g, b, a);

        // Если дошли до конечной точки — выходим из цикла
        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;

        // Корректируем ошибку и координату X
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }

        // Корректируем ошибку и координату Y
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}
EFIAPI void drawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {

    if (!virtual_framebuffer || x >= 1920 || y >= 1080 || w == 0 || h == 0 || a == 0) return;
    if (x + w > 1920) w = 1920 - x;
    if (y + h > 1080) h = 1080 - y;

    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            uint32_t index = (y + i) * 1920 + (x + j);

            if (a == 255) {
                virtual_framebuffer[index].Red   = r;
                virtual_framebuffer[index].Green = g;
                virtual_framebuffer[index].Blue  = b;
            } else {
                uint8_t bg_r = virtual_framebuffer[index].Red;
                uint8_t bg_g = virtual_framebuffer[index].Green;
                uint8_t bg_b = virtual_framebuffer[index].Blue;

                virtual_framebuffer[index].Red   = ((r * a) + (bg_r * (255 - a))) >> 8;
                virtual_framebuffer[index].Green = ((g * a) + (bg_g * (255 - a))) >> 8;
                virtual_framebuffer[index].Blue  = ((b * a) + (bg_b * (255 - a))) >> 8;
            }
        }
    }
}
EFIAPI void fillScreen(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    drawRect(0, 0, 1920, 1080, r, g, b, a);
}
