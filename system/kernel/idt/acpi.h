#pragma once

#include <stdint.h>
#include "../../../efi.h"

// Функция для поиска базовых адресов Local APIC и I/O APIC через ACPI
int init_acpi_apic(BootInfo* info, uint32_t* out_local_apic, uint32_t* out_io_apic);
