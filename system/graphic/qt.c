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

static inline int iabs(int x) { return x < 0 ? -x : x; }

void drawLine(int x0, int y0, int x1, int y1,
                uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int steep = iabs(y1 - y0) > iabs(x1 - x0);
    if (steep) {
        int t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        int t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }

    int dx = x1 - x0;
    int dy = y1 - y0;

    if (dx == 0) {
        drawPoint(steep ? y0 : x0, steep ? x0 : y0, r, g, b, a);
        return;
    }

    /* Фиксированная точка 16.16 */
    int32_t gradient = (dy << 16) / dx;
    int32_t intery = (y0 << 16);

    for (int x = x0; x <= x1; x++) {
        int y = intery >> 16;
        uint8_t frac = (intery >> 8) & 0xFF; /* дробная часть 0..255 */

        uint8_t a1 = ((uint32_t)(255 - frac) * a) / 255;
        uint8_t a2 = ((uint32_t)frac * a) / 255;

        if (steep) {
            if (a1 > 0) drawPoint(y, x, r, g, b, a1);
            if (a2 > 0) drawPoint(y + 1, x, r, g, b, a2);
        } else {
            if (a1 > 0) drawPoint(x, y, r, g, b, a1);
            if (a2 > 0) drawPoint(x, y + 1, r, g, b, a2);
        }
        intery += gradient;
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

static uint32_t isqrt64(uint64_t n) {
    uint64_t x = n, y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return (uint32_t)x;
}

void drawEllipse(int x, int y, int width, int height,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0) return;

    /* Работаем в удвоенных координатах, чтобы избежать половинок для четных размеров */
    int64_t rx = width;   /* 2 * полуось A */
    int64_t ry = height;  /* 2 * полуось B */
    int64_t cx = 2 * x + width - 1;
    int64_t cy = 2 * y + height - 1;

    int64_t rx2 = rx * rx;
    int64_t ry2 = ry * ry;
    int64_t target = rx2 * ry2;
    /* Градиент на границе эллипса для толщины ~1px */
    int64_t norm = 2 * isqrt64((uint64_t)(rx2 * ry2 * (rx2 + ry2)));
    if (norm == 0) norm = 1;

    for (int py = y - 1; py <= y + height; py++) {
        int64_t dy = 2 * py - cy;
        int64_t dy2_rx2 = dy * dy * rx2;
        for (int px = x - 1; px <= x + width; px++) {
            int64_t dx = 2 * px - cx;
            int64_t val = dx * dx * ry2 + dy2_rx2;
            int64_t diff = val - target;
            if (diff < 0) diff = -diff;

            /* Расстояние в субпикселях (0..256) от идеальной кривой */
            int64_t dist = (diff * 256) / norm;
            if (dist < 256) {
                uint8_t alpha = (uint8_t)(((256 - dist) * a) / 256);
                if (alpha > 0) {
                    drawPoint(px, py, r, g, b, alpha);
                }
            }
        }
    }
}
