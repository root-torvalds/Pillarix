#include "ext4_priv.h"

/* Выделение блоков данных по bitmap (он лежит в блоке 3).
 * Возвращает номер блока или 0, если места нет или диск не отвечает. */
uint32_t ext4_alloc_block(void) {
    uint8_t bitmap[EXT4_BLOCK_SIZE];
    if (!ext4_read_block(EXT4_BLOCK_BITMAP_BLOCK, bitmap)) return 0;

    for (uint32_t b = EXT4_ALLOC_START_BLOCK; b < EXT4_BLOCKS_TOTAL; b++) {
        uint8_t mask = (uint8_t)(1 << (b % 8));
        if (bitmap[b / 8] & mask) continue;

        bitmap[b / 8] |= mask;
        if (!ext4_write_block(EXT4_BLOCK_BITMAP_BLOCK, bitmap)) return 0;
        return b;
    }
    return 0;
}
