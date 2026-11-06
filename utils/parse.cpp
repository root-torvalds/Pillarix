#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

struct GlyphData {
    char character;
    int width;
    int rows;
    int bitmap_left;
    int bitmap_top;
    int advance_x;
    unsigned int pixel_offset;
    std::vector<unsigned char> packed_bits;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Использование: " << argv[0] << " <font.ttf> <pixel_size> <output.h>\n";
        return 1;
    }

    std::string font_path = argv[1];
    int pixel_size = std::stoi(argv[2]);
    std::string output_path = argv[3];

    // Инициализация FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Ошибка: не удалось инициализировать FreeType библиотеку\n";
        return 1;
    }

    FT_Face face;
    if (FT_New_Face(ft, font_path.c_str(), 0, &face)) {
        std::cerr << "Ошибка: не удалось загрузить TTF файл: " << font_path << "\n";
        FT_Done_FreeType(ft);
        return 1;
    }

    // Установка размера шрифта (высота в пикселях)
    FT_Set_Pixel_Sizes(face, 0, pixel_size);

    // Набор символов для экспорта (ASCII от пробела до тильды)
    std::string charset = "";
    for (char c = 32; c <= 126; ++c) {
        charset += c;
    }

    std::vector<GlyphData> glyphs;
    std::vector<unsigned char> all_bitmaps;
    unsigned int current_bit_offset = 0;

    for (char c : charset) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, c);
        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
            continue;
        }

        FT_Bitmap& bmp = face->glyph->bitmap;
        int w = bmp.width;
        int r = bmp.rows;

        // Упаковка 8-битного антиалиасинга FreeType в 1 бит на пиксель
        std::vector<unsigned char> char_bits;
        unsigned char current_byte = 0;
        int bit_count = 0;

        for (int y = 0; y < r; ++y) {
            for (int x = 0; x < w; ++x) {
                unsigned char pixel = (x < bmp.width && y < bmp.rows) ? bmp.buffer[y * bmp.pitch + x] : 0;
                if (pixel > 127) {
                    current_byte |= (1 << (7 - bit_count));
                }
                bit_count++;
                if (bit_count == 8) {
                    char_bits.push_back(current_byte);
                    current_byte = 0;
                    bit_count = 0;
                }
            }
        }
        if (bit_count > 0) {
            char_bits.push_back(current_byte);
        }

        // Проверка: существовал ли уже точно такой же битмап (заглушка/прямоугольник)
        bool is_duplicate = false;
        unsigned int duplicate_offset = 0;

        for (const auto& existing : glyphs) {
            if (existing.width == w && existing.rows == r && existing.packed_bits == char_bits) {
                is_duplicate = true;
                duplicate_offset = existing.pixel_offset;
                break;
            }
        }

        GlyphData g;
        g.character = c;
        g.width = w;
        g.rows = r;
        g.bitmap_left = face->glyph->bitmap_left;
        g.bitmap_top = face->glyph->bitmap_top;
        g.advance_x = face->glyph->advance.x >> 6;
        g.packed_bits = char_bits;

        if (is_duplicate) {
            // Если такой символ уже есть в памяти, переиспользуем его смещение
            g.pixel_offset = duplicate_offset;
        } else {
            // Иначе добавляем новый уникальный битмап в общий массив
            g.pixel_offset = current_bit_offset;
            for (unsigned char b : char_bits) {
                all_bitmaps.push_back(b);
            }
            current_bit_offset += (w * r);
        }

        glyphs.push_back(g);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Запись результата в .h файл
    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "Ошибка: не удалось создать выходной файл " << output_path << "\n";
        return 1;
    }

    out << "#pragma once\n\n";
    out << "typedef struct { \n";
    out << "    int width; \n";
    out << "    int rows; \n";
    out << "    int bitmap_left; \n";
    out << "    int bitmap_top; \n";
    out << "    int advance_x; \n";
    out << "    unsigned int pixel_offset; \n";
    out << "} GlyphMetrics; \n\n";

    // Вывод массива байтов битмапов (только уникальные)
    out << "const unsigned char font_bitmaps[] = { \n    ";
    for (size_t i = 0; i < all_bitmaps.size(); ++i) {
        out << "0x" << std::hex << (int)all_bitmaps[i] << std::dec;
        if (i + 1 < all_bitmaps.size()) out << ", ";
        if ((i + 1) % 12 == 0) out << "\n    ";
    }
    out << "\n};\n\n";

    // Вывод функции получения метрик
    out << "GlyphMetrics get_glyph_metrics(char c) { \n";
    out << "    switch(c) { \n";
    for (const auto& g : glyphs) {
        std::string char_repr;
        if (g.character == '\\') char_repr = "'\\\\'";
        else if (g.character == '\'') char_repr = "'\\''";
        else char_repr = std::string("'") + g.character + "'";

        out << "        case " << char_repr << ": return (GlyphMetrics){"
            << g.width << ", " << g.rows << ", " << g.bitmap_left << ", "
            << g.bitmap_top << ", " << g.advance_x << ", " << g.pixel_offset << "}; \n";
    }
    out << "        default: return (GlyphMetrics){0, 0, 0, 0, 0, 0}; \n";
    out << "    } \n";
    out << "}\n";

    out.close();
    std::cout << "Успешно сгенерирован файл с дедупликацией: " << output_path << "\n";
    return 0;
}