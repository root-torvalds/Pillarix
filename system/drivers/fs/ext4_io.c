#include "ext4_priv.h"

/* Блок файловой системы (1 КБ) — это два соседних сектора по 512 байт. */

bool ext4_read_block(uint32_t block, uint8_t* buf) {
    uint64_t lba = (uint64_t)block * EXT4_SECTORS_PER_BLOCK;
    bool first  = ahci_read_sector(lba, buf);
    bool second = ahci_read_sector(lba + 1, buf + 512);
    return first && second;
}

bool ext4_write_block(uint32_t block, const uint8_t* buf) {
    uint64_t lba = (uint64_t)block * EXT4_SECTORS_PER_BLOCK;
    bool first  = ahci_write_sector(lba, buf);
    bool second = ahci_write_sector(lba + 1, buf + 512);
    return first && second;
}
