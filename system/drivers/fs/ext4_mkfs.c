#include "ext4_priv.h"

/* Форматирование диска: пишем суперблок, bitmap'ы, таблицу инодов, журнал
 * и небольшое дерево каталогов, чтобы было что смотреть через ls. */

#define F EXT4_MODE_FILE
#define D EXT4_MODE_DIR

typedef struct {
    uint32_t ino;
    uint16_t mode;
    uint32_t size;
    uint16_t links;
    uint32_t block;     /* первый блок данных, 0 — данных нет */
    uint32_t blocks;
} MkfsInode;

/* Системные иноды 1..11 и заготовки каталогов. Всё, что после 34, свободно. */
static const MkfsInode mkfs_inodes[] = {
    {  1, F, 0,    1,  0,    0 },
    {  2, D, 1024, 11, 1125, 1 },   /* корень */
    {  3, F, 0,    1,  0,    0 },
    {  4, F, 0,    1,  0,    0 },
    {  5, F, 0,    1,  0,    0 },
    {  6, D, 1024, 2,  0,    0 },
    {  7, F, 0,    1,  0,    0 },
    {  8, F, 1024 * EXT4_JOURNAL_BLOCKS, 1, EXT4_JOURNAL_BLOCK, EXT4_JOURNAL_BLOCKS },
    {  9, F, 0,    1,  0,    0 },
    { 10, F, 0,    1,  0,    0 },
    { 11, D, 1024, 2,  1126, 1 },
    { 12, D, 1024, 4,  1127, 1 },   /* /boot */
    { 13, F, 512,  1,  1128, 1 },
    { 14, F, 512,  1,  1129, 1 },
    { 15, D, 1024, 5,  1130, 1 },   /* /system */
    { 16, D, 1024, 2,  1131, 1 },
    { 17, D, 1024, 2,  1132, 1 },
    { 18, D, 1024, 2,  1133, 1 },
    { 19, D, 1024, 4,  1134, 1 },   /* /apps */
    { 20, D, 1024, 2,  1135, 1 },
    { 21, D, 1024, 2,  1136, 1 },
    { 22, D, 1024, 4,  1137, 1 },   /* /config */
    { 23, F, 64,   1,  1138, 1 },
    { 24, F, 64,   1,  1139, 1 },
    { 25, D, 1024, 2,  1140, 1 },
    { 26, D, 1024, 3,  1141, 1 },
    { 27, D, 1024, 2,  1142, 1 },
    { 28, F, 128,  1,  1143, 1 },
    { 29, D, 1024, 4,  1144, 1 },
    { 30, D, 1024, 2,  1145, 1 },
    { 31, D, 1024, 2,  1146, 1 },
    { 32, D, 1024, 4,  1147, 1 },
    { 33, D, 1024, 2,  1148, 1 },   /* /home */
    { 34, D, 1024, 2,  1149, 1 },
};

#undef F
#undef D

/* Таблица инодов собирается целиком в памяти (64 КБ), поэтому static,
 * а не на стеке. */
static uint8_t inode_table[EXT4_INODE_TABLE_BLOCKS * EXT4_BLOCK_SIZE];

static void wipe_blocks(void) {
    uint8_t zero[EXT4_BLOCK_SIZE];
    ext4_zero(zero, sizeof(zero));
    for (uint32_t b = 0; b <= EXT4_MKFS_WIPE_BLOCKS; b++) ext4_write_block(b, zero);
}

static void write_superblock(void) {
    uint8_t buf[EXT4_BLOCK_SIZE];
    ext4_zero(buf, sizeof(buf));

    Ext4SuperBlock* sb = (Ext4SuperBlock*)buf;
    sb->inodecount      = EXT4_INODE_COUNT;
    sb->blockcount      = EXT4_BLOCKS_TOTAL;
    sb->r_blocks_count  = 200;
    sb->free_blockcount = 6988;
    sb->free_inodecount = 222;
    sb->first_data_block = 1;
    sb->log_block_size  = 0;              /* 1024 << 0 */
    sb->log_frag_size   = 0;
    sb->blockper_group  = EXT4_BLOCKS_TOTAL;
    sb->frags_per_group = EXT4_BLOCKS_TOTAL;
    sb->inodeper_group  = EXT4_INODE_COUNT;
    sb->mtime           = 1717200000;
    sb->wtime           = 1717200000;
    sb->mnt_count       = 1;
    sb->max_mnt_count   = 0xFFFF;
    sb->magic           = EXT4_SUPER_MAGIC;
    sb->state           = 1;              /* чистая ФС */
    sb->errors          = 1;
    sb->rev_level       = 1;
    sb->first_ino       = 11;
    sb->inode_size      = EXT4_INODE_SIZE;
    sb->block_group_nr  = 0;

    sb->feature_compat   = 0x003C;                    /* журнал, xattr, resize_inode, dir_index */
    sb->feature_incompat = 0x0002 | 0x0040 | 0x0004;  /* filetype, extents, recover */
    sb->feature_ro_compat = 0x0003;                   /* sparse_super, large_file */

    *(uint32_t*)(buf + 224) = EXT4_JOURNAL_INO;       /* s_journal_inum */

    for (int i = 0; i < 16; i++) sb->uuid[i] = (uint8_t)(i + 1);
    const char label[] = "AnalOS_Ext4";
    for (int i = 0; i < 11; i++) sb->volume_name[i] = label[i];

    ext4_write_block(EXT4_SUPERBLOCK_BLOCK, buf);
}

static void write_group_desc(void) {
    uint8_t buf[EXT4_BLOCK_SIZE];
    ext4_zero(buf, sizeof(buf));

    Ext4GroupDesc* gd = (Ext4GroupDesc*)buf;
    gd->bg_block_bitmap_lo      = EXT4_BLOCK_BITMAP_BLOCK;
    gd->bg_inode_bitmap_lo      = EXT4_INODE_BITMAP_BLOCK;
    gd->bg_inode_table_lo       = EXT4_INODE_TABLE_BLOCK;
    gd->bg_free_blocks_count_lo = 6988;
    gd->bg_free_inodes_count_lo = 222;
    gd->bg_used_dirs_count_lo   = 20;

    ext4_write_block(EXT4_GROUP_DESC_BLOCK, buf);
}

static void write_bitmaps(void) {
    uint8_t buf[EXT4_BLOCK_SIZE];

    /* блоки: всё до EXT4_USED_BLOCKS_END занято */
    ext4_zero(buf, sizeof(buf));
    for (uint32_t b = 0; b <= EXT4_USED_BLOCKS_END; b++) buf[b / 8] |= (uint8_t)(1 << (b % 8));
    ext4_write_block(EXT4_BLOCK_BITMAP_BLOCK, buf);

    /* иноды: заняты 1..34, а всё за пределами 256 помечаем занятым,
     * чтобы никто не полез дальше таблицы */
    ext4_zero(buf, sizeof(buf));
    buf[0] = buf[1] = buf[2] = buf[3] = 0xFF;
    buf[4] = 0x03;
    for (int i = EXT4_INODE_COUNT / 8; i < EXT4_BLOCK_SIZE; i++) buf[i] = 0xFF;
    ext4_write_block(EXT4_INODE_BITMAP_BLOCK, buf);
}

static void write_inode_table(void) {
    ext4_zero(inode_table, sizeof(inode_table));

    uint32_t count = sizeof(mkfs_inodes) / sizeof(mkfs_inodes[0]);
    for (uint32_t i = 0; i < count; i++) {
        const MkfsInode* m = &mkfs_inodes[i];
        Ext4Inode* inode = (Ext4Inode*)(inode_table + (m->ino - 1) * EXT4_INODE_SIZE);

        inode->i_mode        = m->mode;
        inode->i_size_lo     = m->size;
        inode->i_links_count = m->links;
        inode->i_blocks_lo   = m->blocks ? m->blocks * 2 : 2;
        ext4_set_extent(inode, m->block, m->blocks);
    }

    for (uint32_t b = 0; b < EXT4_INODE_TABLE_BLOCKS; b++) {
        ext4_write_block(EXT4_INODE_TABLE_BLOCK + b, inode_table + b * EXT4_BLOCK_SIZE);
    }
}

static void write_directories(void) {
    /* Корень держим коротким, чтобы в его единственном блоке осталось место
     * под файлы, которые создаст пользователь. */
    {
        const char* const names[] = { "boot", "system", "apps", "config", "home" };
        const uint32_t    inos[]  = { 12, 15, 19, 22, 33 };
        const uint8_t     types[] = { 2, 2, 2, 2, 2 };
        ext4_dir_format(EXT4_ROOT_DIR_BLOCK, EXT4_ROOT_INO, EXT4_ROOT_INO, names, inos, types, 5);
    }

    ext4_dir_format(1126, 11, 2, NULL, NULL, NULL, 0);

    {
        const char* const names[] = { "kernel.elf", "initrd.img" };
        const uint32_t    inos[]  = { 13, 14 };
        const uint8_t     types[] = { 1, 1 };
        ext4_dir_format(1127, 12, 2, names, inos, types, 2);
    }
    {
        const char* const names[] = { "bin", "lib" };
        const uint32_t    inos[]  = { 16, 17 };
        const uint8_t     types[] = { 2, 2 };
        ext4_dir_format(1130, 15, 2, names, inos, types, 2);
    }
    ext4_dir_format(1131, 16, 15, NULL, NULL, NULL, 0);
    ext4_dir_format(1132, 17, 15, NULL, NULL, NULL, 0);
    {
        const char* const names[] = { "core", "GUI" };
        const uint32_t    inos[]  = { 20, 21 };
        const uint8_t     types[] = { 2, 2 };
        ext4_dir_format(1134, 19, 2, names, inos, types, 2);
    }
    ext4_dir_format(1135, 20, 19, NULL, NULL, NULL, 0);
    ext4_dir_format(1136, 21, 19, NULL, NULL, NULL, 0);
    {
        const char* const names[] = { "network.conf" };
        const uint32_t    inos[]  = { 23 };
        const uint8_t     types[] = { 1 };
        ext4_dir_format(1137, 22, 2, names, inos, types, 1);
    }
    {
        const char* const names[] = { "user" };
        const uint32_t    inos[]  = { 33 };
        const uint8_t     types[] = { 2 };
        ext4_dir_format(1148, 32, 2, names, inos, types, 1);
    }
}

void mkfs_ext4(void) {
    wipe_blocks();
    write_superblock();
    write_group_desc();
    write_bitmaps();
    write_inode_table();
    ext4_journal_format();
    write_directories();
}
