void* memset(void* ptr, int value, unsigned long num);
void* memcpy(void* dest, const void* src, unsigned long num);
void init_heap(void* free_ram_start, unsigned long free_ram_size);
void* malloc(unsigned long size);
void free(void* ptr);
void* calloc(unsigned long num, unsigned long size);
