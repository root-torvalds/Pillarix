#include <stddef.h>
#include "../font.h"
#include "../kernel/kernel/kernel.h"

/**
 * Отрисовка одного ASCII-символа на экране
 * @param c       - символ для отрисовки
 * @param start_x - базовая координата X (левый край)
 * @param start_y - базовая линия Y (строка, на которой стоят буквы)
 * @param r, g, b - цвет текста
 */
 void print_char(char c, unsigned int start_x, unsigned int start_y,
                unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    GlyphMetrics glyph = get_glyph_metrics(c);

    if (glyph.width == 0 || glyph.rows == 0) {
        return;
    }

    unsigned int glyph_x = start_x + glyph.bitmap_left;
    unsigned int glyph_y = start_y - glyph.bitmap_top;

    for (int row = 0; row < glyph.rows; row++) {
        for (int col = 0; col < glyph.width; col++) {
            size_t pixel_index = glyph.pixel_offset + (row * glyph.width) + col;
            unsigned char alpha = font_bitmaps[pixel_index];

            if (alpha > 0) {
                // Вычисляем итоговую прозрачность пикселя с учетом альфа-канала 'a'
                // Деление на 255 нужно, чтобы значение не вылетело за рамки одного байта
                unsigned int final_alpha = ((unsigned int)alpha * (unsigned int)a) / 255;

                // Рисуем пиксель только в том случае, если итоговая прозрачность больше нуля
                if (final_alpha > 0) {
                    draw_pixel(glyph_x + col, glyph_y + row, r, g, b, (unsigned char)final_alpha);
                }
            }
        }
    }
}

void printf(const void *buffer, unsigned int start_x, unsigned int start_y,
            unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    // Защита от передачи пустого указателя
    if (buffer == (void*)0) return;

    const char *ptr = (const char *)buffer;
    unsigned int current_x = start_x;

    for (int i = 0; ptr[i] != '\0'; i++) {
        char c = ptr[i];

        print_char(c, current_x, start_y, r, g, b, a);

        GlyphMetrics glyph = get_glyph_metrics(c);

        current_x += glyph.advance_x;
    }
}
