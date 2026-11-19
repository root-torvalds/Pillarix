#include <stdint.h>
#include "../kernel/kernel.h"

/* =========================================================================
 * СТРУКТУРЫ ACPI И MADT (описания таблиц самого железа)
 * ========================================================================= */

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

struct MADT {
    struct ACPISDTHeader header;
    uint32_t lica;               // Local Interrupt Controller Address (0xFEE00000)
    uint32_t flags;
} __attribute__((packed));

// Запись Processor Local APIC (Тип 0)
struct MADT_LocalAPIC {
    uint8_t type;
    uint8_t length;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;              // bit 0: Enabled, bit 1: Online Capable
} __attribute__((packed));

// Запись I/O APIC (Тип 1)
struct IO_APIC {
    uint8_t type;
    uint8_t length;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t gsib;
    uint32_t io_apic_address;
} __attribute__((packed));

// Запись Local APIC Address Override (Тип 5)
struct MADT_LAPICOverride {
    uint8_t type;
    uint8_t length;
    uint16_t reserved;
    uint64_t local_apic_address;
} __attribute__((packed));

extern uint64_t local_apic_address;
extern uint64_t io_apic_address;

static uint8_t cpu_apic_ids[MAX_CPUS];
static int cpu_apic_id_to_idx[256];
static int detected_cpu_count = 1;

static int mem_cmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

static void* get_rsdp(EFI_SYSTEM_TABLE *SystemTable) {
    if (!SystemTable) return 0;

    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *table = &SystemTable->ConfigurationTable[i];
        if (table->VendorTable) {
            char* ptr = (char*)table->VendorTable;
            if (mem_cmp(ptr, "RSD PTR ", 8)) {
                return table->VendorTable;
            }
        }
    }
    return 0;
}

static struct ACPISDTHeader* get_system_descriptor_table(struct RSDPDescriptor* rsdp, int* is_xsdt) {
    if (!rsdp) return 0;

    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        *is_xsdt = 1;
        return (struct ACPISDTHeader*)(uintptr_t)rsdp->xsdt_address;
    }

    if (rsdp->rsdt_address != 0) {
        *is_xsdt = 0;
        return (struct ACPISDTHeader*)(uintptr_t)rsdp->rsdt_address;
    }

    return 0;
}

static struct MADT* find_madt(struct ACPISDTHeader* sdt, int is_xsdt) {
    if (!sdt) return 0;

    uint32_t header_size = sizeof(struct ACPISDTHeader);
    uint32_t entry_size = is_xsdt ? 8 : 4;
    uint32_t entries_count = (sdt->length - header_size) / entry_size;

    char* ptrs = (char*)sdt + header_size;

    for (uint32_t i = 0; i < entries_count; i++) {
        uint64_t addr = 0;
        if (is_xsdt) {
            addr = ((uint64_t*)ptrs)[i];
        } else {
            addr = ((uint32_t*)ptrs)[i];
        }

        struct ACPISDTHeader* table_header = (struct ACPISDTHeader*)(uintptr_t)addr;
        if (table_header &&
            table_header->signature[0] == 'A' &&
            table_header->signature[1] == 'P' &&
            table_header->signature[2] == 'I' &&
            table_header->signature[3] == 'C') {
            return (struct MADT*)table_header;
        }
    }

    return 0;
}

void init_apic(BootInfo* info) {
    for (int i = 0; i < 256; i++) cpu_apic_id_to_idx[i] = -1;
    detected_cpu_count = 0;

    uint8_t bsp_id = lapic_get_id();

    if (!info || !info->SystemTable) {
        cpu_apic_ids[0] = bsp_id;
        cpu_apic_id_to_idx[bsp_id] = 0;
        detected_cpu_count = 1;
        return;
    }

    EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE*)info->SystemTable;
    struct RSDPDescriptor* rsdp = (struct RSDPDescriptor*)get_rsdp(st);
    if (!rsdp) goto fallback;

    int is_xsdt = 0;
    struct ACPISDTHeader* sdt = get_system_descriptor_table(rsdp, &is_xsdt);
    if (!sdt) goto fallback;

    struct MADT* madt = find_madt(sdt, is_xsdt);
    if (!madt) goto fallback;

    if (madt->lica != 0) {
        local_apic_address = (uint64_t)madt->lica;
    }

    // Ставим BSP первым (индекс 0)
    cpu_apic_ids[0] = bsp_id;
    cpu_apic_id_to_idx[bsp_id] = 0;
    detected_cpu_count = 1;

    char* current_ptr = (char*)madt + sizeof(struct MADT);
    char* end_ptr = (char*)madt + madt->header.length;

    while (current_ptr < end_ptr) {
        uint8_t entry_type = (uint8_t)current_ptr[0];
        uint8_t entry_length = (uint8_t)current_ptr[1];
        if (entry_length == 0) break;

        if (entry_type == 0 && entry_length >= sizeof(struct MADT_LocalAPIC)) {
            struct MADT_LocalAPIC* lapic_entry = (struct MADT_LocalAPIC*)current_ptr;
            // Проверяем Enabled (bit 0) или Online Capable (bit 1)
            if (lapic_entry->flags & 0x03) {
                uint8_t aid = lapic_entry->apic_id;
                if (aid != bsp_id && cpu_apic_id_to_idx[aid] == -1 && detected_cpu_count < MAX_CPUS) {
                    cpu_apic_ids[detected_cpu_count] = aid;
                    cpu_apic_id_to_idx[aid] = detected_cpu_count;
                    detected_cpu_count++;
                }
            }
        } else if (entry_type == 1 && entry_length >= sizeof(struct IO_APIC)) {
            struct IO_APIC* io_apic = (struct IO_APIC*)current_ptr;
            io_apic_address = io_apic->io_apic_address;
        } else if (entry_type == 5 && entry_length >= sizeof(struct MADT_LAPICOverride)) {
            struct MADT_LAPICOverride* ov = (struct MADT_LAPICOverride*)current_ptr;
            if (ov->local_apic_address != 0) {
                local_apic_address = ov->local_apic_address;
            }
        }

        current_ptr += entry_length;
    }

    if (detected_cpu_count > 0) return;

fallback:
    cpu_apic_ids[0] = bsp_id;
    cpu_apic_id_to_idx[bsp_id] = 0;
    detected_cpu_count = 1;
}

int acpi_get_cpu_count(void) {
    return detected_cpu_count > 0 ? detected_cpu_count : 1;
}

uint8_t acpi_get_apic_id(int cpu_idx) {
    if (cpu_idx < 0 || cpu_idx >= detected_cpu_count) return 0;
    return cpu_apic_ids[cpu_idx];
}

int acpi_find_cpu_by_apic_id(uint8_t apic_id) {
    int idx = cpu_apic_id_to_idx[apic_id];
    return (idx >= 0) ? idx : 0;
}
