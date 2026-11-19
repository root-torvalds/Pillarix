#pragma once
#include <stdint.h>

void disable_pic(void);
void init_local_apic(void);
void init_lapic_timer(uint32_t initial_count);
void io_apic_set_redirect(uint8_t irq, uint8_t vector);
void send_apic_eoi(void);
void init_ioapic(void);

uint8_t lapic_get_id(void);
void lapic_send_ipi(uint8_t apic_id, uint32_t icr_low);
void lapic_send_init(uint8_t apic_id);
void lapic_send_sipi(uint8_t apic_id, uint8_t vector);
