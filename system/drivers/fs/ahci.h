#pragma once

#include <stdint.h>
#include <stdbool.h>

void init_ahci(void);
void ahci_read_sector(uint64_t lba, void* target_buffer);
bool ahci_write_sector(uint64_t lba, const void* source_buffer);
void debug_ahci_status(void);
void flush_cache_line(void* addr, uint32_t length);
void print_hex(uint32_t val, unsigned int x, unsigned int y);
