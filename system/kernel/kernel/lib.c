#include <stdint.h>
#include "kernel.h"

void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

void insb(uint16_t port, void *addr, unsigned long count) {
    __asm__ volatile ("cld; rep insb"
                      : "+D" (addr), "+c" (count)
                      : "d" (port)
                      : "memory");
}

void insw(uint16_t port, void *addr, unsigned long count) {
    __asm__ volatile ("cld; rep insw"
                      : "+D" (addr), "+c" (count)
                      : "d" (port)
                      : "memory");
}

void insl(uint16_t port, void *addr, unsigned long count) {
    __asm__ volatile ("cld; rep insl"
                      : "+D" (addr), "+c" (count)
                      : "d" (port)
                      : "memory");
}

void outsb(uint16_t port, const void *addr, unsigned long count) {
    __asm__ volatile ("cld; rep outsb"
                      : "+S" (addr), "+c" (count)
                      : "d" (port));
}

void outsw(uint16_t port, const void *addr, unsigned long count) {
    __asm__ volatile ("cld; rep outsw"
                      : "+S" (addr), "+c" (count)
                      : "d" (port));
}

void outsl(uint16_t port, const void *addr, unsigned long count) {
    __asm__ volatile ("cld; rep outsl"
                      : "+S" (addr), "+c" (count)
                      : "d" (port));
}

uint32_t strlen(const char* str) {
    uint32_t i = 0;

    while (str[i] != '\0') {
        i++;
    }
    return i;
}
