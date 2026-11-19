#include "qt.h"
#include "../kernel/kernel/kernel.h"

#define QT_ABSOLUTE_SIZE 0
#define QT_RELATIVE_SIZE 1

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

/* Залитый эллипс (и круг) со сглаживанием границы через альфа-канал */
void fillEllipse(int x, int y, int width, int height,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0 || a == 0) return;

    int64_t rx = width;
    int64_t ry = height;
    int64_t cx = 2 * x + width - 1;
    int64_t cy = 2 * y + height - 1;

    int64_t rx2 = rx * rx;
    int64_t ry2 = ry * ry;
    int64_t target = rx2 * ry2;
    int64_t norm = 2 * isqrt64((uint64_t)(rx2 * ry2 * (rx2 + ry2)));
    if (norm == 0) norm = 1;

    for (int py = y - 1; py <= y + height; py++) {
        int64_t dy = 2 * py - cy;
        int64_t dy2_rx2 = dy * dy * rx2;
        for (int px = x - 1; px <= x + width; px++) {
            int64_t dx = 2 * px - cx;
            int64_t val = dx * dx * ry2 + dy2_rx2;

            if (val <= target) {
                drawPoint(px, py, r, g, b, a);
            } else if (val < target + norm) {
                int64_t dist = ((val - target) * 256) / norm;
                uint8_t alpha = (uint8_t)(((256 - dist) * a) / 256);
                if (alpha > 0) drawPoint(px, py, r, g, b, alpha);
            }
        }
    }
}

/* Таблица sin(deg) * 65536 для углов 0..90 градусов */
static const int32_t sin_table_90[91] = {
        0,  1144,  2287,  3430,  4572,  5712,  6850,  7987,  9121, 10252,
    11380, 12505, 13626, 14742, 15855, 16962, 18064, 19161, 20252, 21336,
    22415, 23486, 24550, 25607, 26656, 27697, 28729, 29753, 30767, 31772,
    32768, 33754, 34729, 35693, 36647, 37590, 38521, 39441, 40348, 41243,
    42126, 42995, 43852, 44695, 45525, 46341, 47143, 47930, 48703, 49461,
    50203, 50931, 51643, 52339, 53020, 53684, 54332, 54963, 55578, 56175,
    56756, 57319, 57865, 58393, 58903, 59396, 59870, 60326, 60764, 61183,
    61584, 61966, 62328, 62672, 62997, 63303, 63589, 63856, 64104, 64332,
    64540, 64729, 64898, 65048, 65177, 65287, 65376, 65446, 65496, 65526,
    65536
};

/* Угол передаётся в 1/16 градуса, возвращает sin * 65536 */
static int32_t isin16(int angle16) {
    angle16 %= 5760;
    if (angle16 < 0) angle16 += 5760;

    int sign = 1;
    if (angle16 >= 2880) {
        angle16 -= 2880;
        sign = -1;
    }
    if (angle16 > 1440) {
        angle16 = 2880 - angle16;
    }

    int deg = angle16 >> 4;
    int frac = angle16 & 15;
    if (deg >= 90) return sign * 65536;

    int32_t v0 = sin_table_90[deg];
    int32_t v1 = sin_table_90[deg + 1];
    return sign * (v0 + (((v1 - v0) * frac) >> 4));
}

static int32_t icos16(int angle16) {
    return isin16(angle16 + 1440);
}

/* Возвращает математический угол точки (dx, -dy) в 1/16 градуса [0..5759] */
static int iatan2_16(int64_t dx, int64_t dy_screen) {
    int64_t dy = -dy_screen; /* В Qt 0° справа, 90° сверху */
    if (dx == 0 && dy == 0) return 0;

    int64_t ax = dx < 0 ? -dx : dx;
    int64_t ay = dy < 0 ? -dy : dy;
    int64_t min_v = ax < ay ? ax : ay;
    int64_t max_v = ax > ay ? ax : ay;

    int64_t t = (min_v << 16) / max_v;
    int64_t t2 = (t * t) >> 16;
    int64_t one_m_t2 = 65536 - t2;
    int64_t coeff = 720 + ((163 * one_m_t2) >> 16) - ((43 * ((one_m_t2 * one_m_t2) >> 16)) >> 16);
    int angle = (int)((t * coeff) >> 16);

    if (ay > ax) angle = 1440 - angle;
    if (dx < 0)  angle = 2880 - angle;
    if (dy < 0)  angle = 5760 - angle;

    angle %= 5760;
    return angle < 0 ? angle + 5760 : angle;
}

/* Проверка попадания угла в диапазон [startAngle, startAngle + spanAngle] */
static int angle_in_span(int ang, int start, int span) {
    if (span >= 5760 || span <= -5760) return 1;
    if (span == 0) return 0;

    if (span > 0) {
        int diff = (ang - start) % 5760;
        if (diff < 0) diff += 5760;
        return diff <= span;
    } else {
        int diff = (start - ang) % 5760;
        if (diff < 0) diff += 5760;
        return diff <= -span;
    }
}

/* Прямоугольник со скруглёнными углами (контур со сглаживанием) */
void drawRoundedRect(int x, int y, int width, int height,
                     int xRadius, int yRadius, int mode,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0 || a == 0) return;

    int rx = xRadius;
    int ry = yRadius;
    if (mode == QT_RELATIVE_SIZE) {
        rx = (width * xRadius) / 200;
        ry = (height * yRadius) / 200;
    }
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx > width / 2)  rx = width / 2;
    if (ry > height / 2) ry = height / 2;

    if (rx == 0 || ry == 0) {
        drawLine(x, y, x + width - 1, y, r, g, b, a);
        drawLine(x, y + height - 1, x + width - 1, y + height - 1, r, g, b, a);
        drawLine(x, y, x, y + height - 1, r, g, b, a);
        drawLine(x + width - 1, y, x + width - 1, y + height - 1, r, g, b, a);
        return;
    }

    /* Прямые участки сторон */
    if (width - 2 * rx > 0) {
        drawRect(x + rx, y, width - 2 * rx, 1, r, g, b, a);
        drawRect(x + rx, y + height - 1, width - 2 * rx, 1, r, g, b, a);
    }
    if (height - 2 * ry > 0) {
        drawRect(x, y + ry, 1, height - 2 * ry, r, g, b, a);
        drawRect(x + width - 1, y + ry, 1, height - 2 * ry, r, g, b, a);
    }

    /* 4 сглаженные четверти эллипса для углов */
    int64_t rx2 = (int64_t)(2 * rx) * (2 * rx);
    int64_t ry2 = (int64_t)(2 * ry) * (2 * ry);
    int64_t target = rx2 * ry2;
    int64_t norm = 2 * isqrt64((uint64_t)(rx2 * ry2 * (rx2 + ry2)));
    if (norm == 0) norm = 1;

    int64_t cxs[2] = { 2 * (x + rx) - 1, 2 * (x + width - rx) - 1 };
    int64_t cys[2] = { 2 * (y + ry) - 1, 2 * (y + height - ry) - 1 };

    for (int cy_idx = 0; cy_idx < 2; cy_idx++) {
        int y_start = (cy_idx == 0) ? (y - 1) : (y + height - ry);
        int y_end   = (cy_idx == 0) ? (y + ry - 1) : (y + height);

        for (int cx_idx = 0; cx_idx < 2; cx_idx++) {
            int x_start = (cx_idx == 0) ? (x - 1) : (x + width - rx);
            int x_end   = (cx_idx == 0) ? (x + rx - 1) : (x + width);

            for (int py = y_start; py <= y_end; py++) {
                int64_t dy = 2 * py - cys[cy_idx];
                int64_t dy2_rx2 = dy * dy * rx2;

                for (int px = x_start; px <= x_end; px++) {
                    int64_t dx = 2 * px - cxs[cx_idx];
                    int64_t val = dx * dx * ry2 + dy2_rx2;
                    int64_t diff = val - target;
                    if (diff < 0) diff = -diff;

                    int64_t dist = (diff * 256) / norm;
                    if (dist < 256) {
                        uint8_t alpha = (uint8_t)(((256 - dist) * a) / 256);
                        if (alpha > 0) drawPoint(px, py, r, g, b, alpha);
                    }
                }
            }
        }
    }
}

/* Залитый прямоугольник со скруглёнными углами со сглаживанием */
void fillRoundedRect(int x, int y, int width, int height,
                     int xRadius, int yRadius, int mode,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0 || a == 0) return;

    int rx = xRadius;
    int ry = yRadius;
    if (mode == QT_RELATIVE_SIZE) {
        rx = (width * xRadius) / 200;
        ry = (height * yRadius) / 200;
    }
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx > width / 2)  rx = width / 2;
    if (ry > height / 2) ry = height / 2;

    if (rx == 0 || ry == 0) {
        drawRect(x, y, width, height, r, g, b, a);
        return;
    }

    /* Центральная горизонтальная полоса */
    if (height - 2 * ry > 0) {
        drawRect(x, y + ry, width, height - 2 * ry, r, g, b, a);
    }
    /* Верхний и нижний центральные прямоугольники между углами */
    if (width - 2 * rx > 0) {
        drawRect(x + rx, y, width - 2 * rx, ry, r, g, b, a);
        drawRect(x + rx, y + height - ry, width - 2 * rx, ry, r, g, b, a);
    }

    /* 4 залитых сглаженных сектора для углов */
    int64_t rx2 = (int64_t)(2 * rx) * (2 * rx);
    int64_t ry2 = (int64_t)(2 * ry) * (2 * ry);
    int64_t target = rx2 * ry2;
    int64_t norm = 2 * isqrt64((uint64_t)(rx2 * ry2 * (rx2 + ry2)));
    if (norm == 0) norm = 1;

    int64_t cxs[2] = { 2 * (x + rx) - 1, 2 * (x + width - rx) - 1 };
    int64_t cys[2] = { 2 * (y + ry) - 1, 2 * (y + height - ry) - 1 };

    for (int cy_idx = 0; cy_idx < 2; cy_idx++) {
        int y_start = (cy_idx == 0) ? (y - 1) : (y + height - ry);
        int y_end   = (cy_idx == 0) ? (y + ry - 1) : (y + height);

        for (int cx_idx = 0; cx_idx < 2; cx_idx++) {
            int x_start = (cx_idx == 0) ? (x - 1) : (x + width - rx);
            int x_end   = (cx_idx == 0) ? (x + rx - 1) : (x + width);

            for (int py = y_start; py <= y_end; py++) {
                int64_t dy = 2 * py - cys[cy_idx];
                int64_t dy2_rx2 = dy * dy * rx2;

                for (int px = x_start; px <= x_end; px++) {
                    int64_t dx = 2 * px - cxs[cx_idx];
                    int64_t val = dx * dx * ry2 + dy2_rx2;

                    if (val <= target) {
                        drawPoint(px, py, r, g, b, a);
                    } else if (val < target + norm) {
                        int64_t dist = ((val - target) * 256) / norm;
                        uint8_t alpha = (uint8_t)(((256 - dist) * a) / 256);
                        if (alpha > 0) drawPoint(px, py, r, g, b, alpha);
                    }
                }
            }
        }
    }
}

/* Дуга (контур со сглаживанием) */
void drawArc(int x, int y, int width, int height,
             int startAngle, int spanAngle,
             uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0 || spanAngle == 0 || a == 0) return;

    int64_t rx = width;
    int64_t ry = height;
    int64_t cx = 2 * x + width - 1;
    int64_t cy = 2 * y + height - 1;

    int64_t rx2 = rx * rx;
    int64_t ry2 = ry * ry;
    int64_t target = rx2 * ry2;
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

            int64_t dist = (diff * 256) / norm;
            if (dist < 256) {
                int ang = iatan2_16(dx, dy);
                if (angle_in_span(ang, startAngle, spanAngle)) {
                    uint8_t alpha = (uint8_t)(((256 - dist) * a) / 256);
                    if (alpha > 0) drawPoint(px, py, r, g, b, alpha);
                }
            }
        }
    }
}

/* Сектор («кусок пирога» с заливкой и сглаживанием границ) */
void drawPie(int x, int y, int width, int height,
             int startAngle, int spanAngle,
             uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0 || spanAngle == 0 || a == 0) return;

    int64_t rx = width;
    int64_t ry = height;
    int64_t cx = 2 * x + width - 1;
    int64_t cy = 2 * y + height - 1;

    int64_t rx2 = rx * rx;
    int64_t ry2 = ry * ry;
    int64_t target = rx2 * ry2;
    int64_t norm = 2 * isqrt64((uint64_t)(rx2 * ry2 * (rx2 + ry2)));
    if (norm == 0) norm = 1;

    for (int py = y - 1; py <= y + height; py++) {
        int64_t dy = 2 * py - cy;
        int64_t dy2_rx2 = dy * dy * rx2;
        for (int px = x - 1; px <= x + width; px++) {
            int64_t dx = 2 * px - cx;
            int64_t val = dx * dx * ry2 + dy2_rx2;

            if (val <= target + norm) {
                int ang = iatan2_16(dx, dy);
                if (angle_in_span(ang, startAngle, spanAngle)) {
                    uint8_t alpha = a;
                    if (val > target) {
                        int64_t dist = ((val - target) * 256) / norm;
                        alpha = (uint8_t)(((256 - dist) * a) / 256);
                    }
                    if (alpha > 0) drawPoint(px, py, r, g, b, alpha);
                }
            }
        }
    }

    /* Сглаживаем прямые радиальные стороны сектора */
    if (spanAngle < 5760 && spanAngle > -5760) {
        int endAngle = startAngle + spanAngle;
        int x0 = (int)(cx >> 1);
        int y0 = (int)(cy >> 1);
        int x1 = x0 + (int)(((int64_t)(width / 2) * icos16(startAngle)) >> 16);
        int y1 = y0 - (int)(((int64_t)(height / 2) * isin16(startAngle)) >> 16);
        int x2 = x0 + (int)(((int64_t)(width / 2) * icos16(endAngle)) >> 16);
        int y2 = y0 - (int)(((int64_t)(height / 2) * isin16(endAngle)) >> 16);

        drawLine(x0, y0, x1, y1, r, g, b, a);
        drawLine(x0, y0, x2, y2, r, g, b, a);
    }
}

/* Хорда (сегмент эллипса с заливкой и сглаживанием) */
void drawChord(int x, int y, int width, int height,
               int startAngle, int spanAngle,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0 || spanAngle == 0 || a == 0) return;

    int64_t rx = width;
    int64_t ry = height;
    int64_t cx = 2 * x + width - 1;
    int64_t cy = 2 * y + height - 1;

    int64_t rx2 = rx * rx;
    int64_t ry2 = ry * ry;
    int64_t target = rx2 * ry2;
    int64_t norm = 2 * isqrt64((uint64_t)(rx2 * ry2 * (rx2 + ry2)));
    if (norm == 0) norm = 1;

    /* Вычисляем концы хорды в удвоенных координатах */
    int endAngle = startAngle + spanAngle;
    int64_t px1 = cx + (((int64_t)width * icos16(startAngle)) >> 16);
    int64_t py1 = cy - (((int64_t)height * isin16(startAngle)) >> 16);
    int64_t px2 = cx + (((int64_t)width * icos16(endAngle)) >> 16);
    int64_t py2 = cy - (((int64_t)height * isin16(endAngle)) >> 16);

    /* Вектор секущей линии и её длина для субпиксельного сглаживания */
    int64_t ldx = px2 - px1;
    int64_t ldy = py2 - py1;
    int64_t line_len = (int64_t)isqrt64((uint64_t)(ldx * ldx + ldy * ldy));
    if (line_len == 0) line_len = 1;

    /* Определяем знак полуплоскости для средней точки дуги */
    int midAngle = startAngle + spanAngle / 2;
    int64_t mx = cx + (((int64_t)width * icos16(midAngle)) >> 16);
    int64_t my = cy - (((int64_t)height * isin16(midAngle)) >> 16);
    int64_t mid_cross = ldx * (my - py1) - ldy * (mx - px1);
    int sign = (mid_cross >= 0) ? 1 : -1;

    for (int py = y - 1; py <= y + height; py++) {
        int64_t dy = 2 * py - cy;
        int64_t dy2_rx2 = dy * dy * rx2;
        for (int px = x - 1; px <= x + width; px++) {
            int64_t dx = 2 * px - cx;
            int64_t val = dx * dx * ry2 + dy2_rx2;

            if (val <= target + norm) {
                /* Знаковое расстояние до хорды (в удвоенных пикселях, 2 ед. = 1 px) */
                int64_t cross = sign * (ldx * (2 * py - py1) - ldy * (2 * px - px1));
                if (cross >= -2 * line_len) {
                    uint8_t alpha = a;

                    /* Сглаживание вдоль дуги */
                    if (val > target) {
                        int64_t dist = ((val - target) * 256) / norm;
                        alpha = (uint8_t)(((256 - dist) * alpha) / 256);
                    }
                    /* Сглаживание вдоль прямой хорды */
                    if (cross < 0) {
                        int64_t ldist = (-cross * 256) / (2 * line_len);
                        if (ldist >= 256) alpha = 0;
                        else alpha = (uint8_t)(((256 - ldist) * alpha) / 256);
                    }

                    if (alpha > 0) drawPoint(px, py, r, g, b, alpha);
                }
            }
        }
    }
}
