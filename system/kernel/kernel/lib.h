#pragma once

#include <stdint.h>

uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t val);
uint16_t inw(uint16_t port);
void outw(uint16_t port, uint16_t val);
uint32_t inl(uint16_t port);
void outl(uint16_t port, uint32_t val);

void insb(uint16_t port, void *addr, unsigned long count);
void insw(uint16_t port, void *addr, unsigned long count);
void insl(uint16_t port, void *addr, unsigned long count);
void outsb(uint16_t port, const void *addr, unsigned long count);
void outsw(uint16_t port, const void *addr, unsigned long count);
void outsl(uint16_t port, const void *addr, unsigned long count);
void io_wait(void);
uint32_t strlen(const char* str);
