#include "../kernel/kernel.h"

void* memset(void* ptr, int value, unsigned long num) {
    unsigned char* p = (unsigned char*)ptr;
    for (unsigned long i = 0; i < num; i++) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, unsigned long num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (unsigned long i = 0; i < num; i++) {
        d[i] = s[i];
    }
    return dest;
}

struct block_meta {
    unsigned long size;
    int is_free;
    struct block_meta* next;
};

struct block_meta* heap_start = 0;

void init_heap(void* free_ram_start, unsigned long free_ram_size) {
    heap_start = (struct block_meta*)free_ram_start;
    heap_start->size = free_ram_size - sizeof(struct block_meta);
    heap_start->is_free = 1;
    heap_start->next = 0;
}

void* malloc(unsigned long size) {
    struct block_meta* current = heap_start;
    while (current) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(struct block_meta) + 16) {
                struct block_meta* next_block = (struct block_meta*)((char*)current + sizeof(struct block_meta) + size);
                next_block->size = current->size - size - sizeof(struct block_meta);
                next_block->is_free = 1;
                next_block->next = current->next;

                current->size = size;
                current->next = next_block;
            }
            current->is_free = 0;
            return (void*)(current + 1);
        }
        current = current->next;
    }
    return 0;
}

void free(void* ptr) {
    if (!ptr) return;
    struct block_meta* block = (struct block_meta*)ptr - 1;
    block->is_free = 1;

    if (block->next && block->next->is_free) {
        block->size += sizeof(struct block_meta) + block->next->size;
        block->next = block->next->next;
    }
}

void* calloc(unsigned long num, unsigned long size) {
    unsigned long total = num * size;
    void* ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}
