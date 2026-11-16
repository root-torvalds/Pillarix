#include "ext4_priv.h"

/* Где лежит инод: блок таблицы и смещение внутри него */
static uint32_t inode_block(uint32_t ino) {
    return EXT4_INODE_TABLE_BLOCK + (ino - 1) / EXT4_INODES_PER_BLOCK;
}

static uint32_t inode_offset(uint32_t ino) {
    return ((ino - 1) % EXT4_INODES_PER_BLOCK) * EXT4_INODE_SIZE;
}

bool ext4_read_inode(uint32_t ino, Ext4Inode* out) {
    if (ino == 0 || ino > EXT4_INODE_COUNT) return false;

    uint8_t block[EXT4_BLOCK_SIZE];
    if (!ext4_read_block(inode_block(ino), block)) return false;

    *out = *(Ext4Inode*)(block + inode_offset(ino));
    return true;
}

bool ext4_write_inode(uint32_t ino, const Ext4Inode* inode) {
    if (ino == 0 || ino > EXT4_INODE_COUNT) return false;

    /* Блок общий для четырёх инодов, поэтому читаем, правим свой слот
     * и пишем обратно целиком. Хвост слота за структурой обнуляем. */
    uint8_t block[EXT4_BLOCK_SIZE];
    if (!ext4_read_block(inode_block(ino), block)) return false;

    uint8_t* slot = block + inode_offset(ino);
    ext4_zero(slot, EXT4_INODE_SIZE);
    *(Ext4Inode*)slot = *inode;

    return ext4_write_block(inode_block(ino), block);
}

/* Первый физический блок файла. Мы храним файлы одним экстентом,
 * так что глубже заглядывать не нужно. 0 — блоков нет. */
uint32_t ext4_inode_first_block(const Ext4Inode* inode) {
    const struct ext4_extent_header* hdr = (const struct ext4_extent_header*)inode->i_block;
    if (hdr->eh_magic != EXT4_EXT_MAGIC || hdr->eh_entries == 0) return 0;

    const struct ext4_extent* ext =
        (const struct ext4_extent*)(inode->i_block + sizeof(struct ext4_extent_header));
    return ext->e_ee_start_lo;
}

/* Описывает данные инода одним экстентом: len блоков начиная с start.
 * len == 0 даёт пустой заголовок без экстентов. */
void ext4_set_extent(Ext4Inode* inode, uint32_t start, uint32_t len) {
    struct ext4_extent_header* hdr = (struct ext4_extent_header*)inode->i_block;
    hdr->eh_magic      = EXT4_EXT_MAGIC;
    hdr->eh_entries    = len ? 1 : 0;
    hdr->eh_max        = 4;
    hdr->eh_depth      = 0;
    hdr->eh_generation = 0;

    if (len) {
        struct ext4_extent* ext =
            (struct ext4_extent*)(inode->i_block + sizeof(struct ext4_extent_header));
        ext->e_ee_block    = 0;
        ext->e_ee_len      = (uint16_t)len;
        ext->e_ee_start_lo = start;
        ext->e_ee_start_hi = 0;
    }
    inode->i_flags |= EXT4_EXTENTS_FL;
}
