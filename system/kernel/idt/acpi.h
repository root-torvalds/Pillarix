#pragma once
#include <stdint.h>
#include "../../../efi.h"

#define MAX_CPUS 64

void init_apic(BootInfo* info);
int acpi_get_cpu_count(void);
uint8_t acpi_get_apic_id(int cpu_idx);
int acpi_find_cpu_by_apic_id(uint8_t apic_id);
