#include "ext4_priv.h"

/* Выделение инодов по bitmap в блоке 4. Номера инодов с единицы,
 * а биты в bitmap — с нуля, отсюда ino - 1. */
uint32_t ext4_alloc_inode(void) {
    uint8_t bitmap[EXT4_BLOCK_SIZE];
    if (!ext4_read_block(EXT4_INODE_BITMAP_BLOCK, bitmap)) return 0;

    for (uint32_t ino = EXT4_FIRST_FREE_INO; ino <= EXT4_INODE_COUNT; ino++) {
        uint32_t bit  = ino - 1;
        uint8_t  mask = (uint8_t)(1 << (bit % 8));
        if (bitmap[bit / 8] & mask) continue;

        bitmap[bit / 8] |= mask;
        if (!ext4_write_block(EXT4_INODE_BITMAP_BLOCK, bitmap)) return 0;
        return ino;
    }
    return 0;
}
