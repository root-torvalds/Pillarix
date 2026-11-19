#pragma once
#include <stdint.h>
#include <stddef.h>
#define PX_USER_GFX
#include "px.h"

#define FONT_MAGIC 0x544E4F46u

typedef struct {
    uint8_t Blue;
    uint8_t Green;
    uint8_t Red;
    uint8_t Reserved;
} UserPixel;

typedef struct __attribute__((packed)) {
    int width;
    int rows;
    int bitmap_left;
    int bitmap_top;
    int advance_x;
    uint32_t pixel_offset;
} GlyphMetrics;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t glyph_count;
    uint32_t bitmap_size;
    uint32_t reserved;
} FontHeader;

static UserPixel* user_fb = 0;
static const FontHeader* font_hdr = 0;
static const GlyphMetrics* font_metrics = 0;
static const uint8_t* font_bitmaps = 0;

static inline void font_init_shm(void) {
    if (font_hdr && font_hdr->magic == FONT_MAGIC) return;

    void* shm = sys_shm_map(0);
    if (!shm) {
        int id = sys_shm_alloc(65536);
        if (id < 0) return;
        shm = sys_shm_map(id);
        if (!shm) return;
    }

    FontHeader* hdr = (FontHeader*)shm;
    if (hdr->magic != FONT_MAGIC) {
        static uint8_t tmp[24576];
        int n = fs_read("/system/font.bin", tmp, sizeof(tmp));
        if (n < (int)sizeof(FontHeader)) return;
        uint8_t* dst = (uint8_t*)shm;
        for (int i = 0; i < n; i++) dst[i] = tmp[i];
        if (hdr->magic != FONT_MAGIC) return;
    }

    font_hdr = hdr;
    font_metrics = (const GlyphMetrics*)((const uint8_t*)shm + sizeof(FontHeader));
    font_bitmaps = (const uint8_t*)shm + sizeof(FontHeader) + hdr->glyph_count * sizeof(GlyphMetrics);
}

static inline void gfx_init(void) {
    if (!user_fb) user_fb = (UserPixel*)sys_map_fb();
    font_init_shm();
}

static inline GlyphMetrics get_glyph_metrics(char c) {
    uint8_t uc = (uint8_t)c;
    if (!font_hdr || !font_metrics || uc >= font_hdr->glyph_count) {
        GlyphMetrics empty = {0, 0, 0, 0, 0, 0};
        return empty;
    }
    return font_metrics[uc];
}

static inline void swapBuffers(void) {
    sys_fb_flush();
}

static inline void drawPoint(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!user_fb || x >= 1920 || y >= 1080 || a == 0) return;
    uint32_t idx = y * 1920 + x;
    if (a == 255) {
        user_fb[idx].Red = r; user_fb[idx].Green = g; user_fb[idx].Blue = b;
        return;
    }
    uint8_t br = user_fb[idx].Red, bg = user_fb[idx].Green, bb = user_fb[idx].Blue;
    user_fb[idx].Red   = (uint8_t)(((r * a) + (br * (255 - a))) >> 8);
    user_fb[idx].Green = (uint8_t)(((g * a) + (bg * (255 - a))) >> 8);
    user_fb[idx].Blue  = (uint8_t)(((b * a) + (bb * (255 - a))) >> 8);
}

static inline void drawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!user_fb || x >= 1920 || y >= 1080 || w == 0 || h == 0 || a == 0) return;
    if (x + w > 1920) w = 1920 - x;
    if (y + h > 1080) h = 1080 - y;
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            drawPoint(x + j, y + i, r, g, b, a);
        }
    }
}

static inline void fillScreen(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    drawRect(0, 0, 1920, 1080, r, g, b, a);
}

static inline void drawLine(int x0, int y0, int x1, int y1,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        drawPoint((uint32_t)x0, (uint32_t)y0, r, g, b, a);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static inline uint32_t gfx_isqrt64(uint64_t n) {
    uint64_t x = n, y = (x + 1) >> 1;
    while (y < x) { x = y; y = (x + n / x) >> 1; }
    return (uint32_t)x;
}

static inline void fillRoundedRect(int x, int y, int width, int height,
                                   int xRadius, int yRadius, int mode,
                                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (width <= 0 || height <= 0 || a == 0) return;
    int rx = xRadius, ry = yRadius;
    if (mode == QT_RELATIVE_SIZE) { rx = (width * xRadius) / 200; ry = (height * yRadius) / 200; }
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx > width / 2) rx = width / 2;
    if (ry > height / 2) ry = height / 2;
    if (rx == 0 || ry == 0) { drawRect((uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height, r, g, b, a); return; }

    if (height - 2 * ry > 0) drawRect((uint32_t)x, (uint32_t)(y + ry), (uint32_t)width, (uint32_t)(height - 2 * ry), r, g, b, a);
    if (width - 2 * rx > 0) {
        drawRect((uint32_t)(x + rx), (uint32_t)y, (uint32_t)(width - 2 * rx), (uint32_t)ry, r, g, b, a);
        drawRect((uint32_t)(x + rx), (uint32_t)(y + height - ry), (uint32_t)(width - 2 * rx), (uint32_t)ry, r, g, b, a);
    }
    int64_t rx2 = (int64_t)(2 * rx) * (2 * rx);
    int64_t ry2 = (int64_t)(2 * ry) * (2 * ry);
    int64_t target = rx2 * ry2;
    int64_t norm = 2 * gfx_isqrt64((uint64_t)(rx2 * ry2 * (rx2 + ry2)));
    if (norm == 0) norm = 1;
    int64_t cxs[2] = { 2 * (x + rx) - 1, 2 * (x + width - rx) - 1 };
    int64_t cys[2] = { 2 * (y + ry) - 1, 2 * (y + height - ry) - 1 };

    for (int cyi = 0; cyi < 2; cyi++) {
        int ys = (cyi == 0) ? (y - 1) : (y + height - ry);
        int ye = (cyi == 0) ? (y + ry - 1) : (y + height);
        for (int cxi = 0; cxi < 2; cxi++) {
            int xs = (cxi == 0) ? (x - 1) : (x + width - rx);
            int xe = (cxi == 0) ? (x + rx - 1) : (x + width);
            for (int py = ys; py <= ye; py++) {
                int64_t dy = 2 * py - cys[cyi], dy2 = dy * dy * rx2;
                for (int px = xs; px <= xe; px++) {
                    int64_t dx = 2 * px - cxs[cxi], val = dx * dx * ry2 + dy2;
                    if (val <= target) drawPoint((uint32_t)px, (uint32_t)py, r, g, b, a);
                    else if (val < target + norm) {
                        int64_t dist = ((val - target) * 256) / norm;
                        uint8_t al = (uint8_t)(((256 - dist) * a) / 256);
                        if (al > 0) drawPoint((uint32_t)px, (uint32_t)py, r, g, b, al);
                    }
                }
            }
        }
    }
}

static inline void draw_taskbar(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rad,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    fillRoundedRect((int)x, (int)y, (int)w, (int)h, (int)rad, (int)rad, QT_ABSOLUTE_SIZE, r, g, b, a);
}

static inline void drawChar(char c, uint32_t start_x, uint32_t start_y,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    GlyphMetrics glyph = get_glyph_metrics(c);
    if (glyph.width == 0 || glyph.rows == 0 || !font_bitmaps) return;
    uint32_t gx = start_x + glyph.bitmap_left;
    uint32_t gy = start_y - glyph.bitmap_top;
    for (int row = 0; row < glyph.rows; row++) {
        for (int col = 0; col < glyph.width; col++) {
            uint32_t idx = glyph.pixel_offset + (uint32_t)(row * glyph.width + col);
            if (idx >= font_hdr->bitmap_size) continue;
            uint8_t alpha = font_bitmaps[idx];
            if (alpha > 0) {
                uint8_t fa = (uint8_t)(((uint32_t)alpha * (uint32_t)a) / 255);
                if (fa > 0) drawPoint(gx + (uint32_t)col, gy + (uint32_t)row, r, g, b, fa);
            }
        }
    }
}

static inline void drawText(const char* str, uint32_t start_x, uint32_t start_y,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!str) return;
    uint32_t cx = start_x;
    for (int i = 0; str[i] != '\0'; i++) {
        drawChar(str[i], cx, start_y, r, g, b, a);
        cx += get_glyph_metrics(str[i]).advance_x;
    }
}

static inline uint32_t measureText(const char* str) {
    if (!str) return 0;
    uint32_t w = 0;
    for (int i = 0; str[i] != '\0'; i++) w += get_glyph_metrics(str[i]).advance_x;
    return w;
}