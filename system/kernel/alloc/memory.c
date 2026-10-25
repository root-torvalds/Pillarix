#include <stdint.h>
#include <stdbool.h>

// Простой физический аллокатор страниц (начиная с безопасной зоны 0x200000 -> 2 МБ)
static uint64_t next_free_physical_addr = 0x200000;

void* allocate_physical_page(void) {
    void* ptr = (void*)next_free_physical_addr;
    next_free_physical_addr += 4096; // Шаг страницы 4 КБ
    return ptr;
}

// Аллокатор кучи ядра (простой bump-аллокатор для метаданных и буферов)
static uint64_t kernel_heap_current = 0x4000000; // 64 МБ

void* allocate_kernel_memory(uint32_t size) {
    void* ptr = (void*)kernel_heap_current;
    // Выравниваем размер по границе 16 байт
    uint32_t aligned_size = (size + 15) & ~15;
    kernel_heap_current += aligned_size;
    return ptr;
}

// Низкоуровневый маппинг виртуальной страницы с флагом User/Supervisor (U/S = 1)
// Прямой доступ к текущему PML4 регистру процессора (CR3)
void* kernel_map_page_user(uint64_t virtual_addr, uint64_t physical_addr) {
    uint64_t pml4_addr;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(pml4_addr));

    // Получаем базовый адрес PML4 (сбрасываем управляющие флаги CR3)
    uint64_t* pml4 = (uint64_t*)(pml4_addr & ~0xFFFull);

    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index   = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index   = (virtual_addr >> 12) & 0x1FF;

    // 1. PDPT (Page Directory Pointer Table)
    uint64_t* pdpt;
    if (!(pml4[pml4_index] & 1)) {
        pdpt = (uint64_t*)allocate_physical_page();
        for(int i=0; i<512; i++) pdpt[i] = 0;
        // Устанавливаем флаги Present (1), Writable (2), User (4)
        pml4[pml4_index] = (uint64_t)pdpt | 0x7;
    } else {
        pdpt = (uint64_t*)(pml4[pml4_index] & ~0xFFFull);
    }

    // 2. PD (Page Directory)
    uint64_t* pd;
    if (!(pdpt[pdpt_index] & 1)) {
        pd = (uint64_t*)allocate_physical_page();
        for(int i=0; i<512; i++) pd[i] = 0;
        pdpt[pdpt_index] = (uint64_t)pd | 0x7;
    } else {
        pd = (uint64_t*)(pdpt[pdpt_index] & ~0xFFFull);
    }

    // 3. PT (Page Table)
    uint64_t* pt;
    if (!(pd[pd_index] & 1)) {
        pt = (uint64_t*)allocate_physical_page();
        for(int i=0; i<512; i++) pt[i] = 0;
        pd[pd_index] = (uint64_t)pt | 0x7;
    } else {
        pt = (uint64_t*)(pd[pd_index] & ~0xFFFull);
    }

    // 4. Запись страницы в Page Table с флагом User (бит 2 = 4)
    // 0x7 = Present (1) | Writable (2) | User/Supervisor (4)
    pt[pt_index] = (physical_addr & ~0xFFFull) | 0x7;

    // Сбрасываем TLB для этого виртуального адреса
    __asm__ volatile ("invlpg (%0)" : : "r" (virtual_addr) : "memory");

    return (void*)virtual_addr;
}
