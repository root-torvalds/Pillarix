void disable_pic(void);
void init_local_apic(void);
void init_lapic_timer(uint32_t initial_count);
void io_apic_set_redirect(uint8_t irq, uint8_t vector);
void send_apic_eoi(void);
void init_ioapic(void);

