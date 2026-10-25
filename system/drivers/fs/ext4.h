#pragma once

#include <stdint.h>
#include <stdbool.h>

#define EXT4_SUPER_MAGIC 0xEF53

// Узел экстента Ext4 (заменяет старые прямые блоки)
struct ext4_extent {
    uint32_t e_ee_block; // Первый логический блок файла
    uint16_t e_ee_len;   // Количество блоков (<= 32768)
    uint16_t e_ee_start_hi; // Старшие биты физического адреса
    uint32_t e_ee_start_lo; // Младшие биты физического адреса
} __attribute__((packed));

struct ext4_extent_header {
    uint16_t eh_magic;     // 0xF30A
    uint16_t eh_entries;   // Количество валидных экстентов
    uint16_t eh_max;       // Максимальное количество экстентов
    uint16_t eh_depth;     // Древовидная глубина (0 = листовые экстенты)
    uint32_t eh_generation;// Поколение
} __attribute__((packed));

typedef struct __attribute__((packed)) {
    uint32_t inodecount;
    uint32_t blockcount;
    uint32_t r_blocks_count;
    uint32_t free_blockcount;
    uint32_t free_inodecount;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blockper_group;
    uint32_t frags_per_group;
    uint32_t inodeper_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    // Ext4 специфичные поля расширения
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t  uuid[16];
    char     volume_name[16];
    char     last_mounted[64];
    uint32_t algorithm_usage_bitmap;
} Ext4SuperBlock;

typedef struct __attribute__((packed)) {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
} Ext4GroupDesc;

typedef struct __attribute__((packed)) {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    uint32_t i_osd1;
    // В Ext4 вместо массива block[15] используется структура экстентов i_block[60]
    uint8_t  i_block[60];
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    uint16_t i_blocks_high;
    uint16_t i_file_acl_high;
    uint16_t i_uid_high;
    uint16_t i_gid_high;
    uint16_t i_checksum_lo;
    uint16_t i_reserved;
} Ext4Inode;

typedef struct __attribute__((packed)) {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[255];
} Ext4DirectoryEntry;

// Интерфейсы Ext4 и встроенного mkfs
void mkfs_ext4(void);
void init_ext4(void);
bool ext4_create_file(const char* path, const char* name, uint16_t mode, const void* data, uint32_t data_size);
bool ext4_create_dir(const char* path, const char* name);
uint32_t ext4_find_inode_by_path(const char* path);
