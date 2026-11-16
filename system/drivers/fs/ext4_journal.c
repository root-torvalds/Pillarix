#include "ext4_priv.h"

/* Настоящего журналирования у нас нет: инод 8 просто резервирует место,
 * а здесь пишется суперблок журнала, чтобы том выглядел как ext4 с журналом. */

#define JBD2_MAGIC 0xC03B3998
#define JBD2_SUPERBLOCK_V2 4

/* Поля суперблока журнала хранятся в big-endian */
static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

void ext4_journal_format(void) {
    uint8_t sb[EXT4_BLOCK_SIZE];
    ext4_zero(sb, sizeof(sb));

    put_be32(sb + 0x00, JBD2_MAGIC);
    put_be32(sb + 0x04, JBD2_SUPERBLOCK_V2);
    put_be32(sb + 0x08, 1);                      /* sequence в заголовке */
    put_be32(sb + 0x0C, EXT4_BLOCK_SIZE);
    put_be32(sb + 0x10, EXT4_JOURNAL_BLOCKS);    /* размер журнала в блоках */
    put_be32(sb + 0x14, 1);                      /* первый блок с данными */
    put_be32(sb + 0x18, 1);                      /* sequence первой транзакции */
    put_be32(sb + 0x1C, 0);                      /* start = 0: журнал пуст */
    for (int i = 0; i < 16; i++) sb[0x30 + i] = (uint8_t)(i + 1);   /* uuid */
    put_be32(sb + 0x40, 1);                      /* число пользователей журнала */

    ext4_write_block(EXT4_JOURNAL_BLOCK, sb);
}
