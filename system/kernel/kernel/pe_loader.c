#include "pe_loader.h"

// Функция низкoуровневого маппинга страниц в ядре с флагом User (U/S = 1)
extern void* kernel_map_page_user(uint64_t virtual_addr, uint64_t physical_addr);
extern void* allocate_physical_page(void);

void* allocate_user_pages(uint64_t virtual_addr, uint64_t size) {
    uint64_t num_pages = (size + 4095) / 4096;
    uint64_t current_va = virtual_addr & ~0xFFFull;

    for (uint64_t i = 0; i < num_pages; i++) {
        void* phys = allocate_physical_page();
        if (!phys) return 0;

        // Маппим виртуальный адрес на физический с флагами Present(1), Writable(1), User(1)
        kernel_map_page_user(current_va, (uint64_t)phys);
        current_va += 4096;
    }
    return (void*)virtual_addr;
}

uint64_t load_pe_executable(uint8_t* file_image) {
    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)file_image;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS64* nt_headers = (IMAGE_NT_HEADERS64*)(file_image + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) return 0;

    uint64_t image_base = nt_headers->OptionalHeader.ImageBase;
    uint32_t size_of_image = nt_headers->OptionalHeader.SizeOfImage;

    // Выделяем память под образ с изоляцией Ring 3
    uint8_t* dest_base = (uint8_t*)allocate_user_pages(image_base, size_of_image);
    if (!dest_base) return 0;

    for (uint32_t i = 0; i < nt_headers->OptionalHeader.SizeOfHeaders; i++) {
        dest_base[i] = file_image[i];
    }

    IMAGE_SECTION_HEADER* section = (IMAGE_SECTION_HEADER*)((uint8_t*)&nt_headers->OptionalHeader + nt_headers->FileHeader.SizeOfOptionalHeader);

    for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
        uint8_t* dest_section = dest_base + section[i].VirtualAddress;
        uint8_t* src_section = file_image + section[i].PointerToRawData;

        if (section[i].SizeOfRawData > 0) {
            for (uint32_t j = 0; j < section[i].SizeOfRawData; j++) {
                dest_section[j] = src_section[j];
            }
        }
        if (section[i].Misc.VirtualSize > section[i].SizeOfRawData) {
            for (uint32_t j = section[i].SizeOfRawData; j < section[i].Misc.VirtualSize; j++) {
                dest_section[j] = 0;
            }
        }
    }

    return image_base + nt_headers->OptionalHeader.AddressOfEntryPoint;
}
