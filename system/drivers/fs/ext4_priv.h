#pragma once

/*
 * Внутренний заголовок драйвера ext4: то, что файлам драйвера нужно друг от
 * друга. Публичный интерфейс (структуры на диске и функции для терминала)
 * остался в ext4.h.
 */

#include "../../kernel/kernel/kernel.h"
#include "ext4.h"

/* ---- Раскладка тома ----------------------------------------------------
 * Группа блоков одна, блок 1 КБ, поэтому все адреса зашиты константами. */
#define EXT4_BLOCK_SIZE          1024
#define EXT4_SECTORS_PER_BLOCK   2
#define EXT4_BLOCKS_TOTAL        8192

#define EXT4_INODE_SIZE          256
#define EXT4_INODES_PER_BLOCK    (EXT4_BLOCK_SIZE / EXT4_INODE_SIZE)
#define EXT4_INODE_COUNT         256

#define EXT4_SUPERBLOCK_BLOCK    1
#define EXT4_GROUP_DESC_BLOCK    2
#define EXT4_BLOCK_BITMAP_BLOCK  3
#define EXT4_INODE_BITMAP_BLOCK  4
#define EXT4_INODE_TABLE_BLOCK   5
#define EXT4_INODE_TABLE_BLOCKS  64      /* 256 инодов по 256 байт */

#define EXT4_JOURNAL_BLOCK       69
#define EXT4_JOURNAL_BLOCKS      1056
#define EXT4_ROOT_DIR_BLOCK      1125

/* Всё, что ниже этих номеров, mkfs уже занял под служебные данные и каталоги */
#define EXT4_ALLOC_START_BLOCK   1150
#define EXT4_USED_BLOCKS_END     1205    /* последний занятый блок в bitmap */
#define EXT4_MKFS_WIPE_BLOCKS    1250    /* сколько блоков mkfs затирает нулями */

#define EXT4_ROOT_INO            2
#define EXT4_JOURNAL_INO         8
#define EXT4_FIRST_FREE_INO      35

/* ---- Режимы и типы ---------------------------------------------------- */
#define EXT4_S_IFMT              0xF000
#define EXT4_S_IFDIR             0x4000
#define EXT4_MODE_FILE           0x8180  /* -rw------- */
#define EXT4_MODE_DIR            0x41ED  /* drwxr-xr-x */

#define EXT4_FT_REG              1
#define EXT4_FT_DIR              2

#define EXT4_EXTENTS_FL          0x00080000
#define EXT4_EXT_MAGIC           0xF30A
#define EXT4_DIRENT_HEADER       8       /* inode + rec_len + name_len + type */

/* ---- Коды ошибок ext4_create_file() (их показывает редактор) ---------- */
#define EXT4_E_NO_PARENT         (-1)
#define EXT4_E_PARENT_INODE      (-2)
#define EXT4_E_PARENT_BLOCK      (-3)
#define EXT4_E_DIR_READ          (-4)
#define EXT4_E_FILE_INODE        (-5)
#define EXT4_E_DISK_WRITE        (-6)
#define EXT4_E_INODE_WRITE       (-7)
#define EXT4_E_DIR_FULL          (-12)
#define EXT4_E_NO_INODE          (-14)
#define EXT4_E_BAD_NAME          (-15)
#define EXT4_E_NO_BLOCK          (-16)
#define EXT4_E_TOO_BIG           (-17)

/* Коды ext4_read_file_to_buffer() */
#define EXT4_E_NOENT             (-1)
#define EXT4_E_IO                (-2)
#define EXT4_E_ISDIR             (-3)

static inline void ext4_zero(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    while (n--) *b++ = 0;
}

/* ext4_io.c */
bool ext4_read_block(uint32_t block, uint8_t* buf);
bool ext4_write_block(uint32_t block, const uint8_t* buf);

/* ext4_balloc.c / ext4_ialloc.c */
uint32_t ext4_alloc_block(void);
uint32_t ext4_alloc_inode(void);

/* ext4_inode.c */
bool     ext4_read_inode(uint32_t ino, Ext4Inode* out);
bool     ext4_write_inode(uint32_t ino, const Ext4Inode* inode);
uint32_t ext4_inode_first_block(const Ext4Inode* inode);
void     ext4_set_extent(Ext4Inode* inode, uint32_t start, uint32_t len);

/* ext4_dir.c */
uint32_t ext4_dir_lookup(const uint8_t* dir_buf, const char* name);
bool     ext4_dir_has_room(const uint8_t* dir_buf, int name_len);
uint32_t ext4_dir_add_entry(uint8_t* dir_buf, const char* name, uint32_t ino, uint8_t type);
void     ext4_dir_format(uint32_t block, uint32_t self_ino, uint32_t parent_ino,
                         const char* const names[], const uint32_t inodes[],
                         const uint8_t types[], int count);

/* ext4_namei.c */
int ext4_load_dir(uint32_t dir_ino, Ext4Inode* inode, uint32_t* block, uint8_t* buf);

/* ext4_journal.c */
void ext4_journal_format(void);
