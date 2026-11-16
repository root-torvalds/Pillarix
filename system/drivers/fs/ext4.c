#include "../../kernel/kernel/kernel.h"

static void write_be32(uint8_t *dest, uint32_t val) {
    dest[0] = (val >> 24) & 0xFF;
    dest[1] = (val >> 16) & 0xFF;
    dest[2] = (val >> 8) & 0xFF;
    dest[3] = val & 0xFF;
}

static bool write_block(uint32_t block_num, const uint8_t *buf) {
    bool r1 = ahci_write_sector((uint64_t)block_num * 2, buf);
    bool r2 = ahci_write_sector((uint64_t)block_num * 2 + 1, buf + 512);
    return r1 && r2;
}

static bool read_block(uint32_t block_num, uint8_t *buf) {
    bool r1 = ahci_read_sector((uint64_t)block_num * 2, buf);
    bool r2 = ahci_read_sector((uint64_t)block_num * 2 + 1, buf + 512);
    return r1 && r2;
}

static uint32_t get_inode_block(const Ext4Inode *inode) {
    struct ext4_extent_header *eh = (struct ext4_extent_header *)inode->i_block;
    if (eh->eh_magic == 0xF30A && eh->eh_entries > 0) {
        struct ext4_extent *ext = (struct ext4_extent *)(inode->i_block + sizeof(struct ext4_extent_header));
        return ext->e_ee_start_lo;
    }
    return 0;
}

static bool read_inode(uint32_t inode_num, Ext4Inode *out_inode) {
    if (inode_num == 0 || inode_num > 256) return false;

    uint32_t idx = inode_num - 1;
    uint32_t block_idx = idx / 4;
    uint32_t byte_off = (idx % 4) * 256;

    uint8_t table_block[1024];
    if (!read_block(5 + block_idx, table_block)) return false;

    Ext4Inode *inode = (Ext4Inode *)(table_block + byte_off);
    *out_inode = *inode;
    return true;
}

static bool write_inode_to_disk(uint32_t inode_num, const Ext4Inode *inode) {
    if (inode_num == 0 || inode_num > 256) return false;
    uint32_t idx = inode_num - 1;
    uint32_t block_idx = idx / 4;
    uint32_t byte_off = (idx % 4) * 256;

    uint8_t table_block[1024];
    if (!read_block(5 + block_idx, table_block)) return false;

    uint8_t *target = table_block + byte_off;
    for (int i = 0; i < 256; i++) target[i] = 0;

    *(Ext4Inode *)target = *inode;

    return write_block(5 + block_idx, table_block);
}

// Динамический аллокатор блоков (Block Bitmap на блоке №3)
static uint32_t ext4_allocate_block(void) {
    uint8_t bitmap[1024];
    if (!read_block(3, bitmap)) return 0;

    for (uint32_t b = 1150; b < 8192; b++) {
        uint32_t byte_idx = b / 8;
        uint32_t bit_idx = b % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            bitmap[byte_idx] |= (1 << bit_idx);
            write_block(3, bitmap);
            return b;
        }
    }
    return 0;
}

// Динамический аллокатор инодов (Inode Bitmap на блоке №4)
static uint32_t ext4_allocate_inode(void) {
    uint8_t bitmap[1024];
    if (!read_block(4, bitmap)) return 0;

    for (uint32_t ino = 35; ino <= 256; ino++) {
        uint32_t idx = ino - 1;
        uint32_t byte_idx = idx / 8;
        uint32_t bit_idx = idx % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            bitmap[byte_idx] |= (1 << bit_idx);
            write_block(4, bitmap);
            return ino;
        }
    }
    return 0;
}

static void write_dir_block(uint32_t block_num, uint32_t self_ino, uint32_t parent_ino,
                            const char* names[], const uint32_t inodes[], const uint8_t types[], int count) {
    uint8_t buf[1024];
    for(int i = 0; i < 1024; i++) buf[i] = 0;

    int offset = 0;

    {
        Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(buf + offset);
        de->inode = self_ino;
        de->name_len = 1;
        de->file_type = 2;
        de->name[0] = '.';
        int len = 8 + 1;
        int padded_len = (len + 3) & ~3;
        de->rec_len = padded_len;
        offset += padded_len;
    }

    {
        Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(buf + offset);
        de->inode = parent_ino;
        de->name_len = 2;
        de->file_type = 2;
        de->name[0] = '.';
        de->name[1] = '.';
        int len = 8 + 2;
        int padded_len = (len + 3) & ~3;
        de->rec_len = padded_len;
        offset += padded_len;
    }

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

        if (i == count - 1) {
            padded_len = 1024 - offset; // Занимаем все оставшееся пространство блока до 1024 байт
        }
        de->rec_len = padded_len;
        offset += padded_len;
    }

    write_block(block_num, buf);
                            }

                            static void write_inode_to_table(uint8_t *table_blocks_buf, uint32_t inode_num,
                                                             uint16_t mode, uint32_t size, uint16_t links,
                                                             uint32_t block_start, uint32_t block_len) {
                                uint32_t idx = inode_num - 1;
                                uint32_t block_idx = idx / 4;
                                uint32_t byte_off = (idx % 4) * 256;

                                uint8_t *target = table_blocks_buf + (block_idx * 1024) + byte_off;
                                for(int i = 0; i < 256; i++) target[i] = 0;

                                Ext4Inode *inode = (Ext4Inode *)target;
                                inode->i_mode = mode;
                                inode->i_size_lo = size;
                                inode->i_links_count = links;
                                inode->i_blocks_lo = (block_len > 0) ? (block_len * 2) : 2;
                                inode->i_flags = 0x00080000;

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

                                                             void ext4_split_path(const char* full_path, char* dir_out, char* name_out) {
                                                                 if (!full_path || full_path[0] == '\0') {
                                                                     dir_out[0] = '/';
                                                                     dir_out[1] = '\0';
                                                                     name_out[0] = '\0';
                                                                     return;
                                                                 }

                                                                 int len = 0;
                                                                 while (full_path[len] != '\0') len++;

                                                                 int last_slash = -1;
                                                                 for (int i = len - 1; i >= 0; i--) {
                                                                     if (full_path[i] == '/') {
                                                                         last_slash = i;
                                                                         break;
                                                                     }
                                                                 }

                                                                 if (last_slash < 0) {
                                                                     dir_out[0] = '/';
                                                                     dir_out[1] = '\0';
                                                                     int j = 0;
                                                                     while (full_path[j] != '\0' && j < 255) {
                                                                         name_out[j] = full_path[j];
                                                                         j++;
                                                                     }
                                                                     name_out[j] = '\0';
                                                                 } else if (last_slash == 0) {
                                                                     dir_out[0] = '/';
                                                                     dir_out[1] = '\0';
                                                                     int j = 0;
                                                                     while (full_path[1 + j] != '\0' && j < 255) {
                                                                         name_out[j] = full_path[1 + j];
                                                                         j++;
                                                                     }
                                                                     name_out[j] = '\0';
                                                                 } else {
                                                                     int j = 0;
                                                                     for (j = 0; j < last_slash && j < 299; j++) {
                                                                         dir_out[j] = full_path[j];
                                                                     }
                                                                     dir_out[j] = '\0';

                                                                     j = 0;
                                                                     int start = last_slash + 1;
                                                                     while (full_path[start + j] != '\0' && j < 255) {
                                                                         name_out[j] = full_path[start + j];
                                                                         j++;
                                                                     }
                                                                     name_out[j] = '\0';
                                                                 }
                                                             }

                                                             void mkfs_ext4(void) {
                                                                 uint8_t sector_buffer[1024];
                                                                 for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;

                                                                 for (uint32_t b = 0; b <= 1250; b++) {
                                                                     write_block(b, sector_buffer);
                                                                 }

                                                                 Ext4SuperBlock *sb = (Ext4SuperBlock *)sector_buffer;
                                                                 sb->inodecount = 256;
                                                                 sb->blockcount = 8192;
                                                                 sb->r_blocks_count = 200;
                                                                 sb->free_blockcount = 6988;
                                                                 sb->free_inodecount = 222;
                                                                 sb->first_data_block = 1;
                                                                 sb->log_block_size = 0;
                                                                 sb->log_frag_size = 0;
                                                                 sb->blockper_group = 8192;
                                                                 sb->frags_per_group = 8192;
                                                                 sb->inodeper_group = 256;
                                                                 sb->mtime = 1717200000;
                                                                 sb->wtime = 1717200000;
                                                                 sb->mnt_count = 1;
                                                                 sb->max_mnt_count = 0xFFFF;
                                                                 sb->magic = EXT4_SUPER_MAGIC;
                                                                 sb->state = 1;
                                                                 sb->errors = 1;
                                                                 sb->rev_level = 1;
                                                                 sb->first_ino = 11;
                                                                 sb->inode_size = 256;
                                                                 sb->block_group_nr = 0;

                                                                 sb->feature_compat = 0x003C;
                                                                 sb->feature_incompat = 0x0002 | 0x0040 | 0x0004;
                                                                 sb->feature_ro_compat = 0x0003;

                                                                 *(uint32_t *)(sector_buffer + 224) = 8;

                                                                 for(int i = 0; i < 16; i++) sb->uuid[i] = (uint8_t)(i + 1);
                                                                 char vname[] = "AnalOS_Ext4";
                                                                 for(int i = 0; i < 11; i++) sb->volume_name[i] = vname[i];

                                                                 write_block(1, sector_buffer);

                                                                 for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;

                                                                 *(uint32_t *)(sector_buffer + 0)  = 3;
                                                                 *(uint32_t *)(sector_buffer + 4)  = 4;
                                                                 *(uint32_t *)(sector_buffer + 8)  = 5;
                                                                 *(uint16_t *)(sector_buffer + 12) = 6988;
                                                                 *(uint16_t *)(sector_buffer + 14) = 222;
                                                                 *(uint16_t *)(sector_buffer + 16) = 20;

                                                                 write_block(2, sector_buffer);

                                                                 for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;
                                                                 for (uint32_t b = 0; b <= 1205; b++) {
                                                                     sector_buffer[b / 8] |= (1 << (b % 8));
                                                                 }
                                                                 write_block(3, sector_buffer);

                                                                 for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;
                                                                 sector_buffer[0] = 0xFF;
                                                                 sector_buffer[1] = 0xFF;
                                                                 sector_buffer[2] = 0xFF;
                                                                 sector_buffer[3] = 0xFF;
                                                                 sector_buffer[4] = 0x03;

                                                                 for (int i = 32; i < 1024; i++) {
                                                                     sector_buffer[i] = 0xFF;
                                                                 }
                                                                 write_block(4, sector_buffer);

                                                                 static uint8_t table_blocks[64 * 1024];
                                                                 for (int i = 0; i < 64 * 1024; i++) table_blocks[i] = 0;

                                                                 write_inode_to_table(table_blocks, 1,  0x8180, 0, 1, 0, 0);
                                                                 write_inode_to_table(table_blocks, 2,  0x41ED, 1024, 11, 1125, 1); // Корневой каталог в блоке 1125
                                                                 write_inode_to_table(table_blocks, 3,  0x8180, 0, 1, 0, 0);
                                                                 write_inode_to_table(table_blocks, 4,  0x8180, 0, 1, 0, 0);
                                                                 write_inode_to_table(table_blocks, 5,  0x8180, 0, 1, 0, 0);
                                                                 write_inode_to_table(table_blocks, 6,  0x41ED, 1024, 2, 0, 0);
                                                                 write_inode_to_table(table_blocks, 7,  0x8180, 0, 1, 0, 0);
                                                                 write_inode_to_table(table_blocks, 8,  0x8180, 1024 * 1056, 1, 69, 1056);
                                                                 write_inode_to_table(table_blocks, 9,  0x8180, 0, 1, 0, 0);
                                                                 write_inode_to_table(table_blocks, 10, 0x8180, 0, 1, 0, 0);
                                                                 write_inode_to_table(table_blocks, 11, 0x41ED, 1024, 2, 1126, 1);

                                                                 write_inode_to_table(table_blocks, 12, 0x41ED, 1024, 4, 1127, 1);
                                                                 write_inode_to_table(table_blocks, 13, 0x8180, 512,  1, 1128, 1);
                                                                 write_inode_to_table(table_blocks, 14, 0x8180, 512,  1, 1129, 1);
                                                                 write_inode_to_table(table_blocks, 15, 0x41ED, 1024, 5, 1130, 1);
                                                                 write_inode_to_table(table_blocks, 16, 0x41ED, 1024, 2, 1131, 1);
                                                                 write_inode_to_table(table_blocks, 17, 0x41ED, 1024, 2, 1132, 1);
                                                                 write_inode_to_table(table_blocks, 18, 0x41ED, 1024, 2, 1133, 1);
                                                                 write_inode_to_table(table_blocks, 19, 0x41ED, 1024, 4, 1134, 1);
                                                                 write_inode_to_table(table_blocks, 20, 0x41ED, 1024, 2, 1135, 1);
                                                                 write_inode_to_table(table_blocks, 21, 0x41ED, 1024, 2, 1136, 1);
                                                                 write_inode_to_table(table_blocks, 22, 0x41ED, 1024, 4, 1137, 1);
                                                                 write_inode_to_table(table_blocks, 23, 0x8180, 64,   1, 1138, 1);
                                                                 write_inode_to_table(table_blocks, 24, 0x8180, 64,   1, 1139, 1);
                                                                 write_inode_to_table(table_blocks, 25, 0x41ED, 1024, 2, 1140, 1);
                                                                 write_inode_to_table(table_blocks, 26, 0x41ED, 1024, 3, 1141, 1);
                                                                 write_inode_to_table(table_blocks, 27, 0x41ED, 1024, 2, 1142, 1);
                                                                 write_inode_to_table(table_blocks, 28, 0x8180, 128,  1, 1143, 1);
                                                                 write_inode_to_table(table_blocks, 29, 0x41ED, 1024, 4, 1144, 1);
                                                                 write_inode_to_table(table_blocks, 30, 0x41ED, 1024, 2, 1145, 1);
                                                                 write_inode_to_table(table_blocks, 31, 0x41ED, 1024, 2, 1146, 1);
                                                                 write_inode_to_table(table_blocks, 32, 0x41ED, 1024, 4, 1147, 1);
                                                                 write_inode_to_table(table_blocks, 33, 0x41ED, 1024, 2, 1148, 1);
                                                                 write_inode_to_table(table_blocks, 34, 0x41ED, 1024, 2, 1149, 1);

                                                                 for (int b = 0; b < 64; b++) {
                                                                     write_block(5 + b, table_blocks + (b * 1024));
                                                                 }

                                                                 for (int i = 0; i < 1024; i++) sector_buffer[i] = 0;
                                                                 write_be32(sector_buffer + 0x00, 0xC03B3998);
                                                                 write_be32(sector_buffer + 0x04, 4);
                                                                 write_be32(sector_buffer + 0x08, 1);
                                                                 write_be32(sector_buffer + 0x0C, 1024);
                                                                 write_be32(sector_buffer + 0x10, 1056);
                                                                 write_be32(sector_buffer + 0x14, 1);
                                                                 write_be32(sector_buffer + 0x18, 1);
                                                                 write_be32(sector_buffer + 0x1C, 0);
                                                                 for(int i = 0; i < 16; i++) sector_buffer[0x30 + i] = (uint8_t)(i + 1);
                                                                 write_be32(sector_buffer + 0x40, 1);
                                                                 write_block(69, sector_buffer);

                                                                 {
                                                                     // Оставляем в корне только самые важные папки, чтобы освободить место в блоке 1125
                                                                     const char* names[]  = { "boot", "system", "apps", "config", "home" };
                                                                     const uint32_t inos[] = { 12,     15,       19,     22,       33     };
                                                                     const uint8_t types[] = { 2,      2,        2,      2,        2      };
                                                                     write_dir_block(1125, 2, 2, names, inos, types, 5);
                                                                 }

                                                                 write_dir_block(1126, 11, 2, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "kernel.elf", "initrd.img" };
                                                                     const uint32_t inos[] = { 13,           14           };
                                                                     const uint8_t types[] = { 1,            1            };
                                                                     write_dir_block(1127, 12, 2, names, inos, types, 2);
                                                                 }

                                                                 {
                                                                     const char* names[]  = { "bin", "lib" };
                                                                     const uint32_t inos[] = { 16,    17    };
                                                                     const uint8_t types[] = { 2,     2     };
                                                                     write_dir_block(1130, 15, 2, names, inos, types, 2);
                                                                 }
                                                                 write_dir_block(1131, 16, 15, NULL, NULL, NULL, 0);
                                                                 write_dir_block(1132, 17, 15, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "core", "GUI" };
                                                                     const uint32_t inos[] = { 20,     21   };
                                                                     const uint8_t types[] = { 2,      2    };
                                                                     write_dir_block(1134, 19, 2, names, inos, types, 2);
                                                                 }
                                                                 write_dir_block(1135, 20, 19, NULL, NULL, NULL, 0);
                                                                 write_dir_block(1136, 21, 19, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "network.conf" };
                                                                     const uint32_t inos[] = { 23 };
                                                                     const uint8_t types[] = { 1 };
                                                                     write_dir_block(1137, 22, 2, names, inos, types, 1);
                                                                 }

                                                                 {
                                                                     const char* names[]  = { "user" };
                                                                     const uint32_t inos[] = { 33 };
                                                                     const uint8_t types[] = { 2 };
                                                                     write_dir_block(1148, 32, 2, names, inos, types, 1);
                                                                 }
                                                             }

                                                             int ext4_ls(const char* path, int x, int y) {
                                                                 uint32_t ino = ext4_find_inode_by_path(path);
                                                                 if (ino == 0) {
                                                                     printf("ls: No such file or directory", x, y, 255, 255, 255, 255);
                                                                     return 1;
                                                                 }

                                                                 Ext4Inode inode;
                                                                 if (!read_inode(ino, &inode)) {
                                                                     printf("ls: failed to read inode", x, y, 255, 255, 255, 255);
                                                                     return 1;
                                                                 }

                                                                 if ((inode.i_mode & 0xF000) != 0x4000) {
                                                                     printf(path, x, y, 255, 255, 255, 255);
                                                                     return 1;
                                                                 }

                                                                 uint32_t block_num = get_inode_block(&inode);
                                                                 if (block_num == 0) {
                                                                     return 0;
                                                                 }

                                                                 uint8_t dir_buf[1024];
                                                                 read_block(block_num, dir_buf);

                                                                 uint32_t offset = 0;
                                                                 int current_y = y;
                                                                 int line_height = 20;
                                                                 int lines_printed = 0;

                                                                 while (offset < 1024) {
                                                                     Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(dir_buf + offset);
                                                                     if (de->rec_len == 0 || de->inode == 0) break;

                                                                     char name_buf[256];
                                                                     int len = de->name_len;
                                                                     if (len > 255) len = 255;

                                                                     for (int i = 0; i < len; i++) {
                                                                         name_buf[i] = de->name[i];
                                                                     }
                                                                     name_buf[len] = '\0';

                                                                     char display_buf[300];
                                                                     int idx = 0;
                                                                     while (name_buf[idx] != '\0' && idx < 280) {
                                                                         display_buf[idx] = name_buf[idx];
                                                                         idx++;
                                                                     }

                                                                     if (de->file_type == 2) {
                                                                         display_buf[idx++] = '/';
                                                                     }
                                                                     display_buf[idx] = '\0';

                                                                     printf(display_buf, x, current_y, 255, 255, 255, 255);

                                                                     current_y += line_height;
                                                                     lines_printed++;
                                                                     offset += de->rec_len;
                                                                 }

                                                                 return lines_printed;
                                                             }

                                                             uint32_t ext4_find_inode_by_path(const char* path) {
                                                                 if (!path || path[0] != '/') return 0;
                                                                 if (path[1] == '\0') return 2;

                                                                 char dir_path[300];
                                                                 char file_name[256];
                                                                 ext4_split_path(path, dir_path, file_name);

                                                                 uint32_t parent_ino = 2;
                                                                 if (dir_path[0] != '/' || dir_path[1] != '\0') {
                                                                     parent_ino = ext4_find_inode_by_path(dir_path);
                                                                     if (parent_ino == 0) return 0;
                                                                 }

                                                                 Ext4Inode parent_inode;
                                                                 if (!read_inode(parent_ino, &parent_inode)) return 0;

                                                                 uint32_t parent_block = get_inode_block(&parent_inode);
                                                                 if (parent_block == 0) return 0;

                                                                 uint8_t dir_buf[1024];
                                                                 if (!read_block(parent_block, dir_buf)) return 0;

                                                                 uint32_t offset = 0;
                                                                 while (offset < 1024) {
                                                                     Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(dir_buf + offset);
                                                                     if (de->rec_len == 0 || de->inode == 0) break;

                                                                     bool match = true;
                                                                     int n = 0;
                                                                     while (n < de->name_len && file_name[n] != '\0') {
                                                                         if (de->name[n] != file_name[n]) {
                                                                             match = false;
                                                                             break;
                                                                         }
                                                                         n++;
                                                                     }
                                                                     if (match && de->name_len == n && file_name[n] == '\0') {
                                                                         return de->inode;
                                                                     }

                                                                     offset += de->rec_len;
                                                                 }

                                                                 return 0;
                                                             }

                                                             // Полноценное динамическое создание и перезапись файлов с сохранением родительского инода
                                                             int ext4_create_file(const char* path, const char* name, uint16_t mode, const void* data, uint32_t data_size) {
                                                                 if (name == NULL || name[0] == '\0') return -15;
                                                                 if (data_size > 1024) return -17; // не обрезаем данные молча

                                                                 uint32_t parent_ino = ext4_find_inode_by_path(path);
                                                                 if (parent_ino == 0) return -1;

                                                                 Ext4Inode parent_inode;
                                                                 if (!read_inode(parent_ino, &parent_inode)) return -2;

                                                                 uint32_t parent_block = get_inode_block(&parent_inode);
                                                                 if (parent_block == 0) return -3;

                                                                 uint8_t dir_buf[1024];
                                                                 if (!read_block(parent_block, dir_buf)) return -4;

                                                                 uint32_t offset = 0;
                                                                 uint32_t existing_ino = 0;
                                                                 while (offset < 1024) {
                                                                     Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(dir_buf + offset);
                                                                     if (de->rec_len == 0 || de->inode == 0) break;

                                                                     int match = 1;
                                                                     int n = 0;
                                                                     while (n < de->name_len && name[n] != '\0') {
                                                                         if (de->name[n] != name[n]) { match = 0; break; }
                                                                         n++;
                                                                     }
                                                                     if (match && de->name_len == n && name[n] == '\0') {
                                                                         existing_ino = de->inode;
                                                                         break;
                                                                     }
                                                                     offset += de->rec_len;
                                                                 }

                                                                 uint32_t real_size = data_size;
                                                                 if (real_size == 0 && data != NULL) {
                                                                     const char* p = (const char*)data;
                                                                     while (*p != '\0' && real_size < 1024) {
                                                                         real_size++;
                                                                         p++;
                                                                     }
                                                                 }
                                                                 if (real_size > 1024) real_size = 1024;

                                                                 if (existing_ino != 0) {
                                                                     // Перезапись существующего файла (EDIT)
                                                                     Ext4Inode file_inode;
                                                                     if (!read_inode(existing_ino, &file_inode)) return -5;

                                                                     uint32_t file_block = get_inode_block(&file_inode);
                                                                     if (file_block == 0) {
                                                                         file_block = ext4_allocate_block();
                                                                         if (file_block == 0) return -16;
                                                                     }

                                                                     uint8_t block_buf[1024];
                                                                     for (int i = 0; i < 1024; i++) block_buf[i] = 0;
                                                                     if (data != NULL) {
                                                                         const uint8_t* src = (const uint8_t*)data;
                                                                         for (uint32_t i = 0; i < real_size; i++) block_buf[i] = src[i];
                                                                     }

                                                                     if (!write_block(file_block, block_buf)) return -6;

                                                                     file_inode.i_mode = mode;
                                                                     file_inode.i_size_lo = real_size;
                                                                     file_inode.i_links_count = 1;
                                                                     file_inode.i_blocks_lo = 2;
                                                                     file_inode.i_flags |= 0x00080000;

                                                                     struct ext4_extent_header *eh = (struct ext4_extent_header *)file_inode.i_block;
                                                                     eh->eh_magic = 0xF30A;
                                                                     eh->eh_entries = 1;
                                                                     eh->eh_max = 4;
                                                                     eh->eh_depth = 0;

                                                                     struct ext4_extent *ext = (struct ext4_extent *)(file_inode.i_block + sizeof(struct ext4_extent_header));
                                                                     ext->e_ee_block = 0;
                                                                     ext->e_ee_len = 1;
                                                                     ext->e_ee_start_lo = file_block;
                                                                     ext->e_ee_start_hi = 0;

                                                                     if (!write_inode_to_disk(existing_ino, &file_inode)) return -7;
                                                                     return 0;
                                                                 }

                                                                 // Создание нового файла (TOUCH) в пределах того же блока родительского каталога
                                                                 uint32_t last_offset = 0;
                                                                 offset = 0;
                                                                 while (offset < 1024) {
                                                                     Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(dir_buf + offset);
                                                                     if (de->rec_len == 0 || de->inode == 0) break;
                                                                     last_offset = offset;
                                                                     offset += de->rec_len;
                                                                 }

                                                                 int name_len = 0;
                                                                 while (name[name_len] != '\0') name_len++;

                                                                 Ext4DirectoryEntry *last_de = (Ext4DirectoryEntry *)(dir_buf + last_offset);
                                                                 uint32_t last_actual_len = (8 + last_de->name_len + 3) & ~3u;
                                                                 uint32_t new_entry_offset = last_offset + last_actual_len;
                                                                 uint32_t new_entry_len = (8 + name_len + 3) & ~3u;

                                                                 if (name_len > 255 || new_entry_offset + new_entry_len > 1024) {
                                                                     return -12; // Нет места в блоке директории
                                                                 }

                                                                 uint32_t new_ino = ext4_allocate_inode();
                                                                 if (new_ino == 0) return -14;

                                                                 uint32_t new_block = ext4_allocate_block();
                                                                 if (new_block == 0) return -16;

                                                                 uint8_t block_buf[1024];
                                                                 for (int i = 0; i < 1024; i++) block_buf[i] = 0;
                                                                 if (data != NULL) {
                                                                     for (uint32_t i = 0; i < real_size; i++) block_buf[i] = ((const uint8_t*)data)[i];
                                                                 }
                                                                 if (!write_block(new_block, block_buf)) return -6;

                                                                 Ext4Inode new_inode;
                                                                 for (uint32_t i = 0; i < sizeof(Ext4Inode); i++) ((uint8_t*)&new_inode)[i] = 0;

                                                                 new_inode.i_mode = mode;
                                                                 new_inode.i_size_lo = real_size;
                                                                 new_inode.i_links_count = 1;
                                                                 new_inode.i_blocks_lo = 2;
                                                                 new_inode.i_flags = 0x00080000;

                                                                 struct ext4_extent_header *eh = (struct ext4_extent_header *)new_inode.i_block;
                                                                 eh->eh_magic = 0xF30A;
                                                                 eh->eh_entries = 1;
                                                                 eh->eh_max = 4;
                                                                 eh->eh_depth = 0;

                                                                 struct ext4_extent *ext = (struct ext4_extent *)(new_inode.i_block + sizeof(struct ext4_extent_header));
                                                                 ext->e_ee_block = 0;
                                                                 ext->e_ee_len = 1;
                                                                 ext->e_ee_start_lo = new_block;
                                                                 ext->e_ee_start_hi = 0;

                                                                 if (!write_inode_to_disk(new_ino, &new_inode)) return -7;

                                                                 last_de->rec_len = last_actual_len;

                                                                 Ext4DirectoryEntry *new_de = (Ext4DirectoryEntry *)(dir_buf + new_entry_offset);
                                                                 new_de->inode = new_ino;
                                                                 new_de->name_len = name_len;
                                                                 new_de->file_type = 1;
                                                                 for (int n = 0; n < name_len; n++) new_de->name[n] = name[n];
                                                                 new_de->rec_len = 1024 - new_entry_offset;

                                                                 if (!write_block(parent_block, dir_buf)) return -6;

                                                                 // ВАЖНО: Обновляем и записываем родительский инод на диск, чтобы размер каталога зафиксировался
                                                                 parent_inode.i_size_lo += new_entry_len;
                                                                 if (!write_inode_to_disk(parent_ino, &parent_inode)) return -7;

                                                                 return 0;
                                                             }

                                                             int ext4_cat(const char* path, int x, int y) {
                                                                 uint32_t ino = ext4_find_inode_by_path(path);
                                                                 if (ino == 0) {
                                                                     printf("cat: No such file or directory", x, y, 255, 80, 80, 255);
                                                                     return 1;
                                                                 }

                                                                 Ext4Inode inode;
                                                                 if (!read_inode(ino, &inode)) {
                                                                     printf("cat: Failed to read inode", x, y, 255, 80, 80, 255);
                                                                     return 1;
                                                                 }

                                                                 uint32_t block_num = get_inode_block(&inode);
                                                                 if (block_num == 0) {
                                                                     printf("cat: File has no blocks", x, y, 255, 80, 80, 255);
                                                                     return 1;
                                                                 }

                                                                 uint8_t file_buf[1024];
                                                                 for (int i = 0; i < 1024; i++) file_buf[i] = 0;
                                                                 read_block(block_num, file_buf);

                                                                 uint32_t size = inode.i_size_lo;
                                                                 if (size == 0 || size > 1024) {
                                                                     size = 0;
                                                                     for (uint32_t i = 0; i < 1024; i++) {
                                                                         if (file_buf[i] != 0) {
                                                                             size = i + 1;
                                                                         }
                                                                     }
                                                                 }

                                                                 if (size == 0) {
                                                                     printf("cat: File is empty", x, y, 255, 255, 0, 255);
                                                                     return 1;
                                                                 }

                                                                 int current_y = y;
                                                                 int line_height = 20;
                                                                 int lines_printed = 0;

                                                                 char line_buffer[256];
                                                                 int line_idx = 0;

                                                                 for (uint32_t idx = 0; idx < size; idx++) {
                                                                     char c = (char)file_buf[idx];

                                                                     if (c == '\n' || line_idx >= 254) {
                                                                         line_buffer[line_idx] = '\0';
                                                                         printf(line_buffer, x, current_y, 255, 255, 255, 255);
                                                                         current_y += line_height;
                                                                         lines_printed++;
                                                                         line_idx = 0;
                                                                     } else if (c != '\r' && c >= 32 && c <= 126) {
                                                                         line_buffer[line_idx++] = c;
                                                                     }
                                                                 }

                                                                 if (line_idx > 0) {
                                                                     line_buffer[line_idx] = '\0';
                                                                     printf(line_buffer, x, current_y, 255, 255, 255, 255);
                                                                     current_y += line_height;
                                                                     lines_printed++;
                                                                 }

                                                                 return lines_printed > 0 ? lines_printed : 1;
                                                             }

                                                             bool ext4_create_dir(const char* path, const char* name) {
                                                                 (void)path; (void)name;
                                                                 return true;
                                                             }

                                                             int ext4_read_file_to_buffer(const char* path, void* buf, uint32_t max_size) {
                                                                 uint32_t ino = ext4_find_inode_by_path(path);
                                                                 if (ino == 0) return -1;

                                                                 Ext4Inode inode;
                                                                 if (!read_inode(ino, &inode)) return -2;

                                                                 if ((inode.i_mode & 0xF000) == 0x4000) return -3;

                                                                 uint32_t block_num = get_inode_block(&inode);
                                                                 if (block_num == 0) return 0;

                                                                 uint8_t file_buf[1024];
                                                                 for (int i = 0; i < 1024; i++) file_buf[i] = 0;
                                                                 read_block(block_num, file_buf);

                                                                 uint32_t size = inode.i_size_lo;
                                                                 if (size == 0 || size > 1024) {
                                                                     size = 0;
                                                                     for (uint32_t i = 0; i < 1024; i++) {
                                                                         if (file_buf[i] != 0) size = i + 1;
                                                                     }
                                                                 }

                                                                 if (size > max_size) size = max_size;
                                                                 uint8_t* dest = (uint8_t*)buf;
                                                                 for (uint32_t i = 0; i < size; i++) {
                                                                     dest[i] = file_buf[i];
                                                                 }
                                                                 return (int)size;
                                                             }
