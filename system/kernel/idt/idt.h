#pragma once

#include <stdint.h>

struct IDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));
void init_ioapic(void);
void init_idt(void);