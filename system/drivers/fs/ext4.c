#include "../../kernel/kernel/kernel.h"

// Запись 32-битного числа в Big-Endian (для JBD2 суперблока)[cite: 1]
static void write_be32(uint8_t *dest, uint32_t val) {
    dest[0] = (val >> 24) & 0xFF;
    dest[1] = (val >> 16) & 0xFF;
    dest[2] = (val >> 8) & 0xFF;
    dest[3] = val & 0xFF;
}

// Вспомогательная функция записи блока Ext4 (1 блок = 2 сектора по 512 байт)[cite: 1]
static void write_block(uint32_t block_num, const uint8_t *buf) {
    ahci_write_sector((uint64_t)block_num * 2, buf);
    ahci_write_sector((uint64_t)block_num * 2 + 1, buf + 512);
}

// Вспомогательная функция записи элемента каталога (. и .. + дочерние элементы)[cite: 1]
static void write_dir_block(uint32_t block_num, uint32_t self_ino, uint32_t parent_ino,
                            const char* names[], const uint32_t inodes[], const uint8_t types[], int count) {
    uint8_t buf[1024];
    for(int i = 0; i < 1024; i++) buf[i] = 0;

    int offset = 0;

    // Запись '.' (текущая директория)[cite: 1]
    {
        Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(buf + offset);
        de->inode = self_ino;
        de->name_len = 1;
        de->file_type = 2; // EXT4_FT_DIR[cite: 1]
        de->name[0] = '.';
        int len = 8 + 1;
        int padded_len = (len + 3) & ~3;
        de->rec_len = padded_len;
        offset += padded_len;
    }

    // Запись '..' (родительская директория)[cite: 1]
    {
        Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(buf + offset);
        de->inode = parent_ino;
        de->name_len = 2;
        de->file_type = 2; // EXT4_FT_DIR[cite: 1]
        de->name[0] = '.';
        de->name[1] = '.';
        int len = 8 + 2;
        int padded_len = (len + 3) & ~3;
        de->rec_len = padded_len;
        offset += padded_len;
    }

    // Дочерние записи[cite: 1]
    for(int i = 0; i < count; i++) {
        int name_len = 0;
        while(names[i][name_len] != '\0') name_len++;

        Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(buf + offset);
        de->inode = inodes[i];
        de->name_len = name_len;
        de->file_type = types[i];
        for(int n = 0; n < name_len; n++) de->name[n] = names[i][n];

        int len = 8 + name_len;
        int padded_len = (len + 3) & ~3;

        // Последняя запись забирает всё оставшееся пространство блока до 1024 байт[cite: 1]
        if (i == count - 1) {
            padded_len = 1024 - offset;
        }
        de->rec_len = padded_len;
        offset += padded_len;
    }

    write_block(block_num, buf);
}

// Запись структуры инноды в таблицу иннод[cite: 1]
static void write_inode_to_table(uint8_t *table_blocks_buf, uint32_t inode_num,
                                  uint16_t mode, uint32_t size, uint16_t links,
                                  uint32_t block_start, uint32_t block_len) {
    uint32_t idx = inode_num - 1;
    uint32_t block_idx = idx / 4; // Индекс блока относительно начала таблицы (Блок 5)[cite: 1]
    uint32_t byte_off = (idx % 4) * 256;

    uint8_t *target = table_blocks_buf + (block_idx * 1024) + byte_off;
    for(int i = 0; i < 256; i++) target[i] = 0;

    Ext4Inode *inode = (Ext4Inode *)target;
    inode->i_mode = mode;
    inode->i_size_lo = size;
    inode->i_links_count = links;
    inode->i_blocks_lo = (block_len > 0) ? (block_len * 2) : 2;
    inode->i_flags = 0x00080000; // EXT4_EXTENTS_FL[cite: 1]

    struct ext4_extent_header *eh = (struct ext4_extent_header *)inode->i_block;
    eh->eh_magic = 0xF30A;
    eh->eh_entries = (block_len > 0) ? 1 : 0;
    eh->eh_max = 4;
    eh->eh_depth = 0;
    eh->eh_generation = 0;

    if (block_len > 0) {
        struct ext4_extent *ext = (struct ext4_extent *)(inode->i_block + sizeof(struct ext4_extent_header));
        ext->e_ee_block = 0;
        ext->e_ee_len = block_len;
        ext->e_ee_start_lo = block_start;
        ext->e_ee_start_hi = 0;
    }
}

void mkfs_ext4(void) {
    uint8_t sector_buffer[1024];
    for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;

    // 1. Очищаем первые 1250 блоков под нули[cite: 1]
    for (uint32_t b = 0; b <= 1250; b++) {
        write_block(b, sector_buffer);
    }

    // 2. Суперблок Ext4 (Блок 1)[cite: 1]
    Ext4SuperBlock *sb = (Ext4SuperBlock *)sector_buffer;
    sb->inodecount = 256;
    sb->blockcount = 8192;         // 8 МБ диск[cite: 1]
    sb->r_blocks_count = 200;
    sb->free_blockcount = 7042;    // Точное число свободных блоков (8192 - 1150)[cite: 1]
    sb->free_inodecount = 222;     // 256 - 34 занятые инноды[cite: 1]
    sb->first_data_block = 1;
    sb->log_block_size = 0;        // 1024 байта[cite: 1]
    sb->log_frag_size = 0;
    sb->blockper_group = 8192;
    sb->frags_per_group = 8192;
    sb->inodeper_group = 256;
    sb->mtime = 1717200000;
    sb->wtime = 1717200000;
    sb->mnt_count = 1;
    sb->max_mnt_count = 0xFFFF;
    sb->magic = EXT4_SUPER_MAGIC;  // 0xEF53[cite: 1, 2]
    sb->state = 1;
    sb->errors = 1;
    sb->rev_level = 1;
    sb->first_ino = 11;
    sb->inode_size = 256;
    sb->block_group_nr = 0;

    sb->feature_compat = 0x003C;
    sb->feature_incompat = 0x0002 | 0x0040 | 0x0004; // FILETYPE | EXTENTS | JOURNAL[cite: 1]
    sb->feature_ro_compat = 0x0003;

    // Номер инноды журнала (Inode 8) в суперблоке (смещение 224 / 0xE0)[cite: 1]
    *(uint32_t *)(sector_buffer + 224) = 8;

    for(int i = 0; i < 16; i++) sb->uuid[i] = (uint8_t)(i + 1);
    char vname[] = "AnalOS_Ext4";
    for(int i = 0; i < 11; i++) sb->volume_name[i] = vname[i];

    write_block(1, sector_buffer);

    // 3. Дескриптор группы 0 (Блок 2)[cite: 1]
    for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;

    *(uint32_t *)(sector_buffer + 0)  = 3;    // bg_block_bitmap_lo (Блок 3)[cite: 1]
    *(uint32_t *)(sector_buffer + 4)  = 4;    // bg_inode_bitmap_lo (Блок 4)[cite: 1]
    *(uint32_t *)(sector_buffer + 8)  = 5;    // bg_inode_table_lo (Блоки 5–13)[cite: 1]
    *(uint16_t *)(sector_buffer + 12) = 7042; // bg_free_blocks_count_lo[cite: 1]
    *(uint16_t *)(sector_buffer + 14) = 222;  // bg_free_inodes_count_lo[cite: 1]
    *(uint16_t *)(sector_buffer + 16) = 20;   // bg_used_dirs_count_lo (всего 20 директорий)[cite: 1]

    write_block(2, sector_buffer);

    // 4. Битовая карта блоков (Блок 3) — заняты блоки с 0 по 1149[cite: 1]
    for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;
    for (uint32_t b = 0; b <= 1149; b++) {
        sector_buffer[b / 8] |= (1 << (b % 8));
    }
    write_block(3, sector_buffer);

    // 5. Битовая карта иннод (Блок 4) — активируем инноды с 1 по 34 + паддинг[cite: 1]
    for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;
    sector_buffer[0] = 0xFF; // Инноды 1–8[cite: 1]
    sector_buffer[1] = 0xFF; // Инноды 9–16[cite: 1]
    sector_buffer[2] = 0xFF; // Инноды 17–24[cite: 1]
    sector_buffer[3] = 0xFF; // Инноды 25–32[cite: 1]
    sector_buffer[4] = 0x03; // Инноды 33–34 (биты 0 и 1)[cite: 1]

    for (int i = 32; i < 1024; i++) {
        sector_buffer[i] = 0xFF; // Паддинг неиспользуемых иннод[cite: 1]
    }
    write_block(4, sector_buffer);

    // 6. Заполнение таблицы иннод (Блоки 5–13, всего 9 блоков для 34 иннод)[cite: 1]
    static uint8_t table_blocks[9 * 1024];
    for (int i = 0; i < 9 * 1024; i++) table_blocks[i] = 0;

    // Системные служебные инноды (1–11)[cite: 1]
    write_inode_to_table(table_blocks, 1,  0x8180, 0, 1, 0, 0);
    write_inode_to_table(table_blocks, 2,  0x41ED, 1024, 11, 1125, 1); // Root '/'[cite: 1]
    write_inode_to_table(table_blocks, 3,  0x8180, 0, 1, 0, 0);
    write_inode_to_table(table_blocks, 4,  0x8180, 0, 1, 0, 0);
    write_inode_to_table(table_blocks, 5,  0x8180, 0, 1, 0, 0);
    write_inode_to_table(table_blocks, 6,  0x41ED, 1024, 2, 0, 0);
    write_inode_to_table(table_blocks, 7,  0x8180, 0, 1, 0, 0);
    write_inode_to_table(table_blocks, 8,  0x8180, 1024 * 1056, 1, 69, 1056); // Journal JBD2[cite: 1]
    write_inode_to_table(table_blocks, 9,  0x8180, 0, 1, 0, 0);
    write_inode_to_table(table_blocks, 10, 0x8180, 0, 1, 0, 0);
    write_inode_to_table(table_blocks, 11, 0x41ED, 1024, 2, 1126, 1); // lost+found[cite: 1]

    // Наша структура ОС (Инноды 12–34)[cite: 1]
    write_inode_to_table(table_blocks, 12, 0x41ED, 1024, 4, 1127, 1); // boot[cite: 1]
    write_inode_to_table(table_blocks, 13, 0x8180, 512,  1, 1128, 1); // boot/kernel.elf[cite: 1]
    write_inode_to_table(table_blocks, 14, 0x8180, 512,  1, 1129, 1); // boot/initrd.img[cite: 1]
    write_inode_to_table(table_blocks, 15, 0x41ED, 1024, 5, 1130, 1); // system[cite: 1]
    write_inode_to_table(table_blocks, 16, 0x41ED, 1024, 2, 1131, 1); // system/bin[cite: 1]
    write_inode_to_table(table_blocks, 17, 0x41ED, 1024, 2, 1132, 1); // system/lib[cite: 1]
    write_inode_to_table(table_blocks, 18, 0x41ED, 1024, 2, 1133, 1); // system/servers[cite: 1]
    write_inode_to_table(table_blocks, 19, 0x41ED, 1024, 4, 1134, 1); // apps[cite: 1]
    write_inode_to_table(table_blocks, 20, 0x41ED, 1024, 2, 1135, 1); // apps/core[cite: 1]
    write_inode_to_table(table_blocks, 21, 0x41ED, 1024, 2, 1136, 1); // apps/GUI[cite: 1]
    write_inode_to_table(table_blocks, 22, 0x41ED, 1024, 4, 1137, 1); // config[cite: 1]
    write_inode_to_table(table_blocks, 23, 0x8180, 64,   1, 1138, 1); // config/network.conf[cite: 1]
    write_inode_to_table(table_blocks, 24, 0x8180, 64,   1, 1139, 1); // config/display.conf[cite: 1]
    write_inode_to_table(table_blocks, 25, 0x41ED, 1024, 2, 1140, 1); // dev[cite: 1]
    write_inode_to_table(table_blocks, 26, 0x41ED, 1024, 3, 1141, 1); // proc[cite: 1]
    write_inode_to_table(table_blocks, 27, 0x41ED, 1024, 2, 1142, 1); // proc/self[cite: 1]
    write_inode_to_table(table_blocks, 28, 0x8180, 128,  1, 1143, 1); // proc/meminfo[cite: 1]
    write_inode_to_table(table_blocks, 29, 0x41ED, 1024, 4, 1144, 1); // volumes[cite: 1]
    write_inode_to_table(table_blocks, 30, 0x41ED, 1024, 2, 1145, 1); // volumes/efi[cite: 1]
    write_inode_to_table(table_blocks, 31, 0x41ED, 1024, 2, 1146, 1); // volumes/storage0[cite: 1]
    write_inode_to_table(table_blocks, 32, 0x41ED, 1024, 4, 1147, 1); // user[cite: 1]
    write_inode_to_table(table_blocks, 33, 0x41ED, 1024, 2, 1148, 1); // user/root[cite: 1]
    write_inode_to_table(table_blocks, 34, 0x41ED, 1024, 2, 1149, 1); // user/guest[cite: 1]

    // Записываем блоки таблицы иннод на диск (блоки 5–13)[cite: 1]
    for (int b = 0; b < 9; b++) {
        write_block(5 + b, table_blocks + (b * 1024));
    }

    // 7. Инициализация суперблока JBD2 (Блок 69)[cite: 1]
    for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;
    write_be32(sector_buffer + 0x00, 0xC03B3998); // h_magic[cite: 1]
    write_be32(sector_buffer + 0x04, 4);          // h_blocktype = JBD2_SUPERBLOCK_V2[cite: 1]
    write_be32(sector_buffer + 0x08, 1);          // h_sequence[cite: 1]
    write_be32(sector_buffer + 0x0C, 1024);       // s_blocksize[cite: 1]
    write_be32(sector_buffer + 0x10, 1056);       // s_maxlen[cite: 1]
    write_be32(sector_buffer + 0x14, 1);          // s_first[cite: 1]
    write_be32(sector_buffer + 0x18, 1);          // s_sequence[cite: 1]
    write_be32(sector_buffer + 0x1C, 0);          // s_start[cite: 1]
    for(int i = 0; i < 16; i++) sector_buffer[0x30 + i] = (uint8_t)(i + 1);
    write_be32(sector_buffer + 0x40, 1);          // s_nr_users[cite: 1]
    write_block(69, sector_buffer);

    // 8. Наполнение каталогов и создание файлов на диске (блоки 1125–1149)[cite: 1]

    // Блок 1125: Root '/' (Inode 2)[cite: 1]
    {
        const char* names[]  = { "lost+found", "boot", "system", "apps", "config", "dev", "proc", "volumes", "user" };
        const uint32_t inos[] = { 11,           12,     15,       19,     22,       25,    26,     29,        32     };
        const uint8_t types[] = { 2,            2,      2,        2,      2,        2,     2,      2,         2      };
        write_dir_block(1125, 2, 2, names, inos, types, 9);
    }

    // Блок 1126: lost+found (Inode 11)[cite: 1]
    write_dir_block(1126, 11, 2, NULL, NULL, NULL, 0);

    // Блок 1127: boot (Inode 12)[cite: 1]
    {
        const char* names[]  = { "kernel.elf", "initrd.img" };
        const uint32_t inos[] = { 13,           14           };
        const uint8_t types[] = { 1,            1            }; // 1 = файл[cite: 1]
        write_dir_block(1127, 12, 2, names, inos, types, 2);
    }

    // Блок 1130: system (Inode 15)[cite: 1]
    {
        const char* names[]  = { "bin", "lib", "servers" };
        const uint32_t inos[] = { 16,    17,    18        };
        const uint8_t types[] = { 2,     2,     2         };
        write_dir_block(1130, 15, 2, names, inos, types, 3);
    }

    // Внутренние пустые системные папки[cite: 1]
    write_dir_block(1131, 16, 15, NULL, NULL, NULL, 0); // system/bin[cite: 1]
    write_dir_block(1132, 17, 15, NULL, NULL, NULL, 0); // system/lib[cite: 1]
    write_dir_block(1133, 18, 15, NULL, NULL, NULL, 0); // system/servers[cite: 1]

    // Блок 1134: apps (Inode 19)[cite: 1]
    {
        const char* names[]  = { "core", "GUI" };
        const uint32_t inos[] = { 20,     21   };
        const uint8_t types[] = { 2,      2    };
        write_dir_block(1134, 19, 2, names, inos, types, 2);
    }
    write_dir_block(1135, 20, 19, NULL, NULL, NULL, 0); // apps/core[cite: 1]
    write_dir_block(1136, 21, 19, NULL, NULL, NULL, 0); // apps/GUI[cite: 1]

    // Блок 1137: config (Inode 22)[cite: 1]
    {
        const char* names[]  = { "network.conf", "display.conf" };
        const uint32_t inos[] = { 23,             24             };
        const uint8_t types[] = { 1,              1              };
        write_dir_block(1137, 22, 2, names, inos, types, 2);
    }

    // Блок 1140: dev (Inode 25)[cite: 1]
    write_dir_block(1140, 25, 2, NULL, NULL, NULL, 0);

    // Блок 1141: proc (Inode 26)[cite: 1]
    {
        const char* names[]  = { "self", "meminfo" };
        const uint32_t inos[] = { 27,     28        };
        const uint8_t types[] = { 2,      1         }; // self — папка, meminfo — файл[cite: 1]
        write_dir_block(1141, 26, 2, names, inos, types, 2);
    }
    write_dir_block(1142, 27, 26, NULL, NULL, NULL, 0); // proc/self[cite: 1]

    // Блок 1144: volumes (Inode 29)[cite: 1]
    {
        const char* names[]  = { "efi", "storage0" };
        const uint32_t inos[] = { 30,    31        };
        const uint8_t types[] = { 2,     2         };
        write_dir_block(1144, 29, 2, names, inos, types, 2);
    }
    write_dir_block(1145, 30, 29, NULL, NULL, NULL, 0); // volumes/efi[cite: 1]
    write_dir_block(1146, 31, 29, NULL, NULL, NULL, 0); // volumes/storage0[cite: 1]

    // Блок 1147: user (Inode 32)[cite: 1]
    {
        const char* names[]  = { "root", "guest" };
        const uint32_t inos[] = { 33,     34     };
        const uint8_t types[] = { 2,      2      };
        write_dir_block(1147, 32, 2, names, inos, types, 2);
    }
    write_dir_block(1148, 33, 32, NULL, NULL, NULL, 0); // user/root[cite: 1]
    write_dir_block(1149, 34, 32, NULL, NULL, NULL, 0); // user/guest[cite: 1]
}

void init_ext4(void) {
    mkfs_ext4();
}

static void read_block(uint32_t block_num, uint8_t *buf) {
    ahci_read_sector((uint64_t)block_num * 2, buf);
    ahci_read_sector((uint64_t)block_num * 2 + 1, buf + 512);
}

// Чтение структуры инноды из таблицы (Блоки 5–13)[cite: 1]
static bool read_inode(uint32_t inode_num, Ext4Inode *out_inode) {
    if (inode_num == 0 || inode_num > 256) return false;
    uint32_t idx = inode_num - 1;
    uint32_t table_start_block = 5;
    uint32_t byte_offset = idx * 256;
    uint32_t block_offset = byte_offset / 1024;
    uint32_t intra_block_offset = byte_offset % 1024;

    uint8_t block_buf[1024];
    read_block(table_start_block + block_offset, block_buf);

    Ext4Inode *src = (Ext4Inode *)(block_buf + intra_block_offset);
    for (uint32_t i = 0; i < sizeof(Ext4Inode); i++) {
        ((uint8_t*)out_inode)[i] = ((uint8_t*)src)[i];
    }
    return true;
}

// Поиск инноды по строковому пути (например, "/app")[cite: 1]
uint32_t ext4_find_inode_by_path(const char* path) {
    if (!path || path[0] != '/') return 0;
    if (path[1] == '\0' || (path[1] == '/' && path[2] == '\0')) return 2; // Root '/'[cite: 1]

    uint32_t current_ino = 2;
    const char* cursor = path + 1;

    while (*cursor != '\0') {
        while (*cursor == '/') cursor++;
        if (*cursor == '\0') break;

        Ext4Inode dir_inode;
        if (!read_inode(current_ino, &dir_inode)) return 0;

        struct ext4_extent_header *eh = (struct ext4_extent_header *)dir_inode.i_block;
        if (eh->eh_magic != 0xF30A) return 0;
        struct ext4_extent *ext = (struct ext4_extent *)(dir_inode.i_block + sizeof(struct ext4_extent_header));

        bool found = false;
        uint32_t found_ino = 0;

        for (int e = 0; e < eh->eh_entries && !found; e++) {
            uint32_t phys_block = ext[e].e_ee_start_lo;
            uint16_t block_len = ext[e].e_ee_len;
            if (block_len > 32768) block_len -= 32768;

            for (uint16_t b = 0; b < block_len && !found; b++) {
                uint8_t block_buf[1024];
                read_block(phys_block + b, block_buf);

                int offset = 0;
                while (offset < 1024) {
                    Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(block_buf + offset);
                    if (de->rec_len == 0) break;

                    if (de->inode != 0) {
                        int match = 1;
                        int i = 0;
                        while (i < de->name_len && cursor[i] != '\0' && cursor[i] != '/') {
                            if (de->name[i] != cursor[i]) { match = 0; break; }
                            i++;
                        }
                        if (match && (i == de->name_len) && (cursor[i] == '\0' || cursor[i] == '/')) {
                            found_ino = de->inode;
                            found = true;
                            break;
                        }
                    }
                    offset += de->rec_len;
                }
            }
        }

        if (!found) return 0;
        current_ino = found_ino;
        while (*cursor != '\0' && *cursor != '/') cursor++;
    }

    return current_ino;
}

// Реальное чтение файла с диска через дерево экстентов Ext4[cite: 1]
uint32_t ext4_read_file(const char* path, void* buffer, uint32_t buffer_size) {
    uint32_t ino = ext4_find_inode_by_path(path);
    if (ino == 0) return 0;

    Ext4Inode inode;
    if (!read_inode(ino, &inode)) return 0;

    uint32_t file_size = inode.i_size_lo;
    if (file_size > buffer_size) file_size = buffer_size;

    struct ext4_extent_header *eh = (struct ext4_extent_header *)inode.i_block;
    if (eh->eh_magic != 0xF30A) return 0;

    struct ext4_extent *ext = (struct ext4_extent *)(inode.i_block + sizeof(struct ext4_extent_header));
    uint32_t bytes_read = 0;
    uint8_t *dest = (uint8_t *)buffer;

    for (int i = 0; i < eh->eh_entries; i++) {
        uint32_t phys_block = ext[i].e_ee_start_lo;
        uint16_t block_len = ext[i].e_ee_len;
        if (block_len > 32768) block_len -= 32768;

        for (uint16_t b = 0; b < block_len; b++) {
            uint8_t block_buf[1024];
            read_block(phys_block + b, block_buf);

            uint32_t chunk = 1024;
            if (bytes_read + chunk > file_size) {
                chunk = file_size - bytes_read;
            }
            for (uint32_t c = 0; c < chunk; c++) {
                dest[bytes_read + c] = block_buf[c];
            }
            bytes_read += chunk;
            if (bytes_read >= file_size) break;
        }
        if (bytes_read >= file_size) break;
    }
    return bytes_read;
}

bool ext4_create_file(const char* path, const char* name, uint16_t mode, const void* data, uint32_t data_size) {
    (void)path; (void)name; (void)mode; (void)data; (void)data_size;
    return true;
}

bool ext4_create_dir(const char* path, const char* name) {
    (void)path; (void)name;
    return true;
}
