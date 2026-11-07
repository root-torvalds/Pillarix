#include "../../kernel/kernel/kernel.h"

static void write_be32(uint8_t *dest, uint32_t val) {
    dest[0] = (val >> 24) & 0xFF;
    dest[1] = (val >> 16) & 0xFF;
    dest[2] = (val >> 8) & 0xFF;
    dest[3] = val & 0xFF;
}

static void write_block(uint32_t block_num, const uint8_t *buf) {
    ahci_write_sector((uint64_t)block_num * 2, buf);
    ahci_write_sector((uint64_t)block_num * 2 + 1, buf + 512);
}

static void read_block(uint32_t block_num, uint8_t *buf) {
    ahci_read_sector((uint64_t)block_num * 2, buf);
    ahci_read_sector((uint64_t)block_num * 2 + 1, buf + 512);
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
    read_block(5 + block_idx, table_block);

    Ext4Inode *inode = (Ext4Inode *)(table_block + byte_off);
    *out_inode = *inode;
    return true;
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
            padded_len = 1024 - offset;
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
                                                                 for (uint32_t b = 0; b <= 1203; b++) {
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
                                                                 write_inode_to_table(table_blocks, 2,  0x41ED, 1024, 11, 1125, 1);
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
                                                                     const char* names[]  = { "lost+found", "boot", "system", "apps", "config", "dev", "proc", "volumes", "user" };
                                                                     const uint32_t inos[] = { 11,           12,     15,       19,     22,       25,    26,     29,        32     };
                                                                     const uint8_t types[] = { 2,            2,      2,        2,      2,        2,     2,      2,         2      };
                                                                     write_dir_block(1125, 2, 2, names, inos, types, 9);
                                                                 }

                                                                 write_dir_block(1126, 11, 2, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "kernel.elf", "initrd.img" };
                                                                     const uint32_t inos[] = { 13,           14           };
                                                                     const uint8_t types[] = { 1,            1            };
                                                                     write_dir_block(1127, 12, 2, names, inos, types, 2);
                                                                 }

                                                                 {
                                                                     const char* names[]  = { "bin", "lib", "servers" };
                                                                     const uint32_t inos[] = { 16,    17,    18        };
                                                                     const uint8_t types[] = { 2,     2,     2         };
                                                                     write_dir_block(1130, 15, 2, names, inos, types, 3);
                                                                 }
                                                                 write_dir_block(1131, 16, 15, NULL, NULL, NULL, 0);
                                                                 write_dir_block(1132, 17, 15, NULL, NULL, NULL, 0);
                                                                 write_dir_block(1133, 18, 15, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "core", "GUI" };
                                                                     const uint32_t inos[] = { 20,     21   };
                                                                     const uint8_t types[] = { 2,      2    };
                                                                     write_dir_block(1134, 19, 2, names, inos, types, 2);
                                                                 }
                                                                 write_dir_block(1135, 20, 19, NULL, NULL, NULL, 0);
                                                                 write_dir_block(1136, 21, 19, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "network.conf", "display.conf" };
                                                                     const uint32_t inos[] = { 23,             24             };
                                                                     const uint8_t types[] = { 1,              1              };
                                                                     write_dir_block(1137, 22, 2, names, inos, types, 2);
                                                                 }

                                                                 write_dir_block(1140, 25, 2, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "self", "meminfo" };
                                                                     const uint32_t inos[] = { 27,     28        };
                                                                     const uint8_t types[] = { 2,      1         };
                                                                     write_dir_block(1141, 26, 2, names, inos, types, 2);
                                                                 }
                                                                 write_dir_block(1142, 27, 26, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "efi", "storage0" };
                                                                     const uint32_t inos[] = { 30,    31        };
                                                                     const uint8_t types[] = { 2,     2         };
                                                                     write_dir_block(1144, 29, 2, names, inos, types, 2);
                                                                 }
                                                                 write_dir_block(1145, 30, 29, NULL, NULL, NULL, 0);
                                                                 write_dir_block(1146, 31, 29, NULL, NULL, NULL, 0);

                                                                 {
                                                                     const char* names[]  = { "root", "guest" };
                                                                     const uint32_t inos[] = { 33,     34     };
                                                                     const uint8_t types[] = { 2,      2      };
                                                                     write_dir_block(1147, 32, 2, names, inos, types, 2);
                                                                 }
                                                                 write_dir_block(1148, 33, 32, NULL, NULL, NULL, 0);
                                                                 write_dir_block(1149, 34, 32, NULL, NULL, NULL, 0);
                                                             }

                                                             uint32_t ext4_find_inode_by_path(const char* path) {
                                                                 if (!path || path[0] != '/') return 0;
                                                                 if (path[1] == '\0') return 2;

                                                                 const char *name_ptr = path + 1;
                                                                 Ext4Inode root_inode;
                                                                 if (!read_inode(2, &root_inode)) return 0;

                                                                 uint32_t root_block = get_inode_block(&root_inode);
                                                                 if (root_block == 0) return 0;

                                                                 uint8_t dir_buf[1024];
                                                                 read_block(root_block, dir_buf);

                                                                 uint32_t offset = 0;
                                                                 while (offset < 1024) {
                                                                     Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(dir_buf + offset);
                                                                     if (de->rec_len == 0 || de->inode == 0) break;

                                                                     bool match = true;
                                                                     for (int i = 0; i < de->name_len; i++) {
                                                                         if (name_ptr[i] != de->name[i] || name_ptr[i] == '\0') {
                                                                             match = false;
                                                                             break;
                                                                         }
                                                                     }
                                                                     if (match && name_ptr[de->name_len] == '\0' && de->name_len > 0) {
                                                                         return de->inode;
                                                                     }

                                                                     offset += de->rec_len;
                                                                 }

                                                                 return 0;
                                                             }

                                                             bool ext4_create_file(const char* path, const char* name, uint16_t mode, const void* data, uint32_t data_size) {
                                                                 uint32_t parent_ino = ext4_find_inode_by_path(path);
                                                                 if (parent_ino == 0) parent_ino = 2;

                                                                 Ext4Inode parent_inode;
                                                                 if (!read_inode(parent_ino, &parent_inode)) return false;
                                                                 uint32_t parent_block = get_inode_block(&parent_inode);
                                                                 if (parent_block == 0) return false;

                                                                 uint8_t dir_buf[1024];
                                                                 read_block(parent_block, dir_buf);

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

                                                                 uint32_t max_blk = 1203;
                                                                 for (uint32_t i = 1; i <= 256; i++) {
                                                                     Ext4Inode inv;
                                                                     if (read_inode(i, &inv) && inv.i_links_count > 0) {
                                                                         uint32_t blk = get_inode_block(&inv);
                                                                         if (blk > max_blk) max_blk = blk;
                                                                     }
                                                                 }
                                                                 uint32_t target_block = max_blk + 1;

                                                                 if (existing_ino != 0) {
                                                                     Ext4Inode file_inode;
                                                                     if (!read_inode(existing_ino, &file_inode)) return false;
                                                                     uint32_t file_block = get_inode_block(&file_inode);
                                                                     if (file_block == 0) {
                                                                         file_block = target_block;
                                                                     }

                                                                     uint8_t block_buf[1024];
                                                                     for(int i = 0; i < 1024; i++) block_buf[i] = 0;
                                                                     if (data && data_size > 0) {
                                                                         uint32_t copy_size = data_size > 1024 ? 1024 : data_size;
                                                                         for(uint32_t i = 0; i < copy_size; i++) block_buf[i] = ((const uint8_t*)data)[i];
                                                                     }
                                                                     write_block(file_block, block_buf);

                                                                     uint32_t idx = existing_ino - 1;
                                                                     uint32_t block_idx = idx / 4;
                                                                     uint32_t byte_off = (idx % 4) * 256;
                                                                     uint8_t table_block[1024];
                                                                     read_block(5 + block_idx, table_block);

                                                                     Ext4Inode *ino_ptr = (Ext4Inode *)(table_block + byte_off);
                                                                     ino_ptr->i_mode = mode;
                                                                     ino_ptr->i_size_lo = data_size;
                                                                     ino_ptr->i_size_high = 0;
                                                                     ino_ptr->i_links_count = 1;
                                                                     ino_ptr->i_blocks_lo = 2;
                                                                     ino_ptr->i_flags = 0x00080000;

                                                                     struct ext4_extent_header *eh = (struct ext4_extent_header *)ino_ptr->i_block;
                                                                     eh->eh_magic = 0xF30A;
                                                                     eh->eh_entries = 1;
                                                                     eh->eh_max = 4;
                                                                     eh->eh_depth = 0;
                                                                     eh->eh_generation = 0;

                                                                     struct ext4_extent *ext = (struct ext4_extent *)(ino_ptr->i_block + sizeof(struct ext4_extent_header));
                                                                     ext->e_ee_block = 0;
                                                                     ext->e_ee_len = 1;
                                                                     ext->e_ee_start_lo = file_block;
                                                                     ext->e_ee_start_hi = 0;

                                                                     write_block(5 + block_idx, table_block);
                                                                     return true;
                                                                 } else {
                                                                     uint32_t new_ino = 35;
                                                                     for (uint32_t i = 35; i <= 256; i++) {
                                                                         Ext4Inode test_inode;
                                                                         if (read_inode(i, &test_inode) && test_inode.i_links_count == 0) {
                                                                             new_ino = i;
                                                                             break;
                                                                         }
                                                                     }

                                                                     uint32_t new_block = target_block;

                                                                     uint8_t block_buf[1024];
                                                                     for(int i = 0; i < 1024; i++) block_buf[i] = 0;
                                                                     if (data && data_size > 0) {
                                                                         uint32_t copy_size = data_size > 1024 ? 1024 : data_size;
                                                                         for(uint32_t i = 0; i < copy_size; i++) block_buf[i] = ((const uint8_t*)data)[i];
                                                                     }
                                                                     write_block(new_block, block_buf);

                                                                     uint32_t idx = new_ino - 1;
                                                                     uint32_t block_idx = idx / 4;
                                                                     uint32_t byte_off = (idx % 4) * 256;
                                                                     uint8_t table_block[1024];
                                                                     read_block(5 + block_idx, table_block);

                                                                     Ext4Inode *inode = (Ext4Inode *)(table_block + byte_off);
                                                                     inode->i_mode = mode;
                                                                     inode->i_size_lo = data_size;
                                                                     inode->i_size_high = 0;
                                                                     inode->i_links_count = 1;
                                                                     inode->i_blocks_lo = 2;
                                                                     inode->i_flags = 0x00080000;

                                                                     struct ext4_extent_header *eh = (struct ext4_extent_header *)inode->i_block;
                                                                     eh->eh_magic = 0xF30A;
                                                                     eh->eh_entries = 1;
                                                                     eh->eh_max = 4;
                                                                     eh->eh_depth = 0;
                                                                     eh->eh_generation = 0;

                                                                     struct ext4_extent *ext = (struct ext4_extent *)(inode->i_block + sizeof(struct ext4_extent_header));
                                                                     ext->e_ee_block = 0;
                                                                     ext->e_ee_len = 1;
                                                                     ext->e_ee_start_lo = new_block;
                                                                     ext->e_ee_start_hi = 0;
                                                                     write_block(5 + block_idx, table_block);

                                                                     offset = 0;
                                                                     uint32_t last_offset = 0;
                                                                     while (offset < 1024) {
                                                                         Ext4DirectoryEntry *de = (Ext4DirectoryEntry *)(dir_buf + offset);
                                                                         if (de->rec_len == 0 || de->inode == 0) break;
                                                                         last_offset = offset;
                                                                         offset += de->rec_len;
                                                                     }

                                                                     if (last_offset > 0) {
                                                                         Ext4DirectoryEntry *last_de = (Ext4DirectoryEntry *)(dir_buf + last_offset);
                                                                         int actual_len = (8 + last_de->name_len + 3) & ~3;
                                                                         last_de->rec_len = actual_len;

                                                                         uint32_t new_entry_offset = last_offset + actual_len;
                                                                         int name_len = 0;
                                                                         while (name[name_len] != '\0') name_len++;

                                                                         Ext4DirectoryEntry *new_de = (Ext4DirectoryEntry *)(dir_buf + new_entry_offset);
                                                                         new_de->inode = new_ino;
                                                                         new_de->name_len = name_len;
                                                                         new_de->file_type = 1;
                                                                         for (int n = 0; n < name_len; n++) new_de->name[n] = name[n];
                                                                         new_de->rec_len = 1024 - new_entry_offset;

                                                                         write_block(parent_block, dir_buf);
                                                                     }
                                                                     return true;
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

                                                                 if ((inode.i_mode & 0xF000) == 0x4000) {
                                                                     printf("cat: Is a directory", x, y, 255, 80, 80, 255);
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
                                                                         if ((file_buf[i] >= 32 && file_buf[i] <= 126) || file_buf[i] == '\n' || file_buf[i] == '\t') {
                                                                             size = i + 1;
                                                                         }
                                                                     }
                                                                 }

                                                                 if (size == 0) {
                                                                     for (uint32_t i = 0; i < 1024; i++) {
                                                                         if (file_buf[i] == '\0') {
                                                                             size = i;
                                                                             break;
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
                                                                     lines_printed++;
                                                                 }

                                                                 return lines_printed > 0 ? lines_printed : 1;
                                                             }

                                                             bool ext4_create_dir(const char* path, const char* name) {
                                                                 (void)path; (void)name;
                                                                 return true;
                                                             }
