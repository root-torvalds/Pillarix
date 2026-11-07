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
    std::vector<unsigned char> pixels;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Использование: " << argv[0] << " <font.ttf> <pixel_size> <output.h>\n";
        return 1;
    }

    std::string font_path = argv[1];
    int pixel_size = std::stoi(argv[2]);
    std::string output_path = argv[3];

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Ошибка: не удалось инициализировать FreeType\n";
        return 1;
    }

    FT_Face face;
    if (FT_New_Face(ft, font_path.c_str(), 0, &face)) {
        std::cerr << "Ошибка: не удалось загрузить TTF файл\n";
        FT_Done_FreeType(ft);
        return 1;
    }

    FT_Set_Pixel_Sizes(face, 0, pixel_size);

    std::string charset = "";
    for (char c = 32; c <= 126; ++c) {
        charset += c;
    }

    std::vector<GlyphData> glyphs;
    std::vector<unsigned char> all_bitmaps;
    unsigned int current_byte_offset = 0;

    const int TARGET_WIDTH = 15; // Жесткая ширина 15 пикселей

    for (char c : charset) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, c);
        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
            continue;
        }

        FT_Bitmap& bmp = face->glyph->bitmap;
        int r = bmp.rows;

        // Сохраняем по 1 байту на пиксель (0-255), строка за строкой, с фиксированной шириной 15
        std::vector<unsigned char> char_pixels;
        for (int y = 0; y < r; ++y) {
            for (int x = 0; x < TARGET_WIDTH; ++x) {
                unsigned char pixel = 0;
                if (x < bmp.width && y < bmp.rows) {
                    pixel = bmp.buffer[y * bmp.pitch + x];
                }
                char_pixels.push_back(pixel);
            }
        }

        // Проверка на дубликаты (дедупликация заглушек)
        bool is_duplicate = false;
        unsigned int duplicate_offset = 0;

        for (const auto& existing : glyphs) {
            if (existing.rows == r && existing.pixels == char_pixels) {
                is_duplicate = true;
                duplicate_offset = existing.pixel_offset;
                break;
            }
        }

        GlyphData g;
        g.character = c;
        g.width = TARGET_WIDTH;
        g.rows = r;
        g.bitmap_left = face->glyph->bitmap_left;
        g.bitmap_top = face->glyph->bitmap_top;
        g.advance_x = TARGET_WIDTH;
        g.pixels = char_pixels;

        if (is_duplicate) {
            g.pixel_offset = duplicate_offset;
        } else {
            g.pixel_offset = current_byte_offset;
            for (unsigned char p : char_pixels) {
                all_bitmaps.push_back(p);
            }
            current_byte_offset += char_pixels.size();
        }

        glyphs.push_back(g);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Запись в .h файл
    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "Ошибка создания файла " << output_path << "\n";
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

    out << "const unsigned char font_bitmaps[] = { \n    ";
    for (size_t i = 0; i < all_bitmaps.size(); ++i) {
        out << "0x" << std::hex << (int)all_bitmaps[i] << std::dec;
        if (i + 1 < all_bitmaps.size()) out << ", ";
        if ((i + 1) % 16 == 0) out << "\n    ";
    }
    out << "\n};\n\n";

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
    std::cout << "Успешно сгенерирован файл с байтовым форматом: " << output_path << "\n";
    return 0;
}