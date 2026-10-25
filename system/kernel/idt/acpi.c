#include <stdint.h>
#include "../kernel/kernel.h"

/* =========================================================================
 * СТРУКТУРЫ ACPI И MADT (описания таблиц самого железа)
 * ========================================================================= */

// 1. Дескриптор RSDP (ищем сигнатуру "RSD PTR ")
struct RSDPDescriptor {
    char signature[8];
    uint8_t checksum;
    uint8_t oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

// 2. Стандартный заголовок таблиц ACPI
struct ACPISDTHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oemid[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

// 3. Главный заголовок MADT (Таблица 5.19)
struct MADT {
    struct ACPISDTHeader header;
    uint32_t lica;               // Local Interrupt Controller Address (0xFEE00000)
    uint32_t flags;
} __attribute__((packed));

// 4. Запись I/O APIC (Тип 1)
struct IO_APIC {
    uint8_t type;
    uint8_t length;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t gsib;
    uint32_t io_apic_address;    // Физический адрес I/O APIC (обычно 0xFEC00000)
} __attribute__((packed));

/* =========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ И ПАРСЕР
 * ========================================================================= */

static int mem_cmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

// Функция поиска RSDP через системную таблицу UEFI
struct RSDPDescriptor* find_rsdp(BootInfo* info) {
    if (!info) {
        return 0;
    }

    // Используем готовый тип EFI_SYSTEM_TABLE из твоих подключаемых заголовочных файлов (efi.h)
    // Внимание: если в твоем BootInfo поле называется иначе (например, SystemTable),
    // исправь EfiSystemTable на нужное имя.
    EFI_SYSTEM_TABLE* st = (EFI_SYSTEM_TABLE*)info->SystemTable;
    if (!st) return 0;

    // Перебираем все конфигурационные таблицы UEFI с помощью счетчика
    for (uint64_t i = 0; i < st->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE* table = &st->ConfigurationTable[i];

        if (table->VendorTable) {
            char* ptr = (char*)table->VendorTable;
            // Ищем сигнатуру "RSD PTR "
            if (mem_cmp(ptr, "RSD PTR ", 8)) {
                return (struct RSDPDescriptor*)ptr;
            }
        }
    }
    return 0;
}

// Функция для поиска XSDT или RSDT на основе найденного RSDP
struct ACPISDTHeader* get_system_descriptor_table(struct RSDPDescriptor* rsdp) {
    if (!rsdp) {
        return 0; // Защита от нулевого указателя
    }

    // Шаг 1: Проверяем, поддерживает ли система ACPI 2.0+ и заполнена ли XSDT
    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        // XSDT является предпочтительной для 64-битных систем
        return (struct ACPISDTHeader*)(uintptr_t)rsdp->xsdt_address;
    }

    // Шаг 2: Резервный вариант для старых систем (ACPI 1.0) — используем RSDT
    if (rsdp->rsdt_address != 0) {
        return (struct ACPISDTHeader*)(uintptr_t)rsdp->rsdt_address;
    }

    return 0; // Если адресов нет
}

// Функция для поиска MADT по сигнатуре "APIC" внутри XSDT/RSDT
struct MADT* find_madt(struct ACPISDTHeader* system_descriptor_table) {
    if (!system_descriptor_table) {
        return 0;
    }

    // Вычисляем количество таблиц в XSDT.
    // Каждый указатель на таблицу в XSDT занимает 8 байт (uint64_t).
    uint32_t header_size = sizeof(struct ACPISDTHeader);
    uint32_t entries_count = (system_descriptor_table->length - header_size) / 8;

    // Указатель на массив адресов начинается сразу после заголовка XSDT
    uint64_t* table_pointers = (uint64_t*)((char*)system_descriptor_table + header_size);

    // Перебираем все указатели в поисках MADT ("APIC")
    for (uint32_t i = 0; i < entries_count; i++) {
        struct ACPISDTHeader* table_header = (struct ACPISDTHeader*)(uintptr_t)table_pointers[i];

        if (table_header) {
            // Проверяем сигнатуру таблицы на равенство "APIC"
            if (table_header->signature[0] == 'A' &&
                table_header->signature[1] == 'P' &&
                table_header->signature[2] == 'I' &&
                table_header->signature[3] == 'C') {

                // Нашли MADT! Возвращаем как структуру MADT
                return (struct MADT*)table_header;
            }
        }
    }

    return 0; // Если таблица MADT не найдена
}
// Функция для поиска RSDP в таблицах UEFI конфигурации
void* get_rsdp(EFI_SYSTEM_TABLE *SystemTable) {
    if (!SystemTable) return 0;

    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *table = &SystemTable->ConfigurationTable[i];
        if (table->VendorTable) {
            char* ptr = (char*)table->VendorTable;
            // Ищем заветную сигнатуру "RSD PTR "
            if (mem_cmp(ptr, "RSD PTR ", 8)) {
                return table->VendorTable;
            }
        }
    }
    return 0;
}
// Главная функция инициализации APIC и поиска оборудования
void init_apic(BootInfo* info) {
    if (!info || !info->SystemTable) {
        return;
    }

    // Шаг 1: Получаем системную таблицу UEFI
    EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE*)info->SystemTable;

    // Шаг 2: Находим RSDP
    struct RSDPDescriptor* rsdp = (struct RSDPDescriptor*)get_rsdp(st);
    if (!rsdp) {
        return;
    }

    // Шаг 3: Находим XSDT или RSDT
    struct ACPISDTHeader* sdt = get_system_descriptor_table(rsdp);
    if (!sdt) {
        return;
    }

    // Шаг 4: Находим MADT по сигнатуре "APIC"
    struct MADT* madt = find_madt(sdt);
    if (!madt) {
        return;
    }

    // Шаг 5: Обходим переменные записи внутри MADT
    char* current_ptr = (char*)madt + sizeof(struct MADT);
    char* end_ptr = (char*)madt + madt->header.length;

    while (current_ptr < end_ptr) {
        uint8_t entry_type = current_ptr[0];
        uint8_t entry_length = current_ptr[1];

        // Защита от зависания при нулевой длине записи
        if (entry_length == 0) {
            break;
        }

        // Тип 1: I/O APIC (контроллер прерываний ввода-вывода)
        if (entry_type == 1) {
            struct IO_APIC* io_apic = (struct IO_APIC*)current_ptr;

            // Здесь мы получаем физический адрес I/O APIC (обычно 0xFEC00000)
            uint32_t io_apic_address = io_apic->io_apic_address;

            // Пока мы можем сохранить его в глобальную переменную или просто использовать
            (void)io_apic_address;
        }

        // Тип 0: Processor Local APIC (процессорное ядро)
        else if (entry_type == 0) {
            // Здесь содержится информация о ядрах процессора
            // (пригодится нам позже для многоядерности - SMP)
        }

        // Переходим к следующей записи с помощью шага length
        current_ptr += entry_length;
    }
}
