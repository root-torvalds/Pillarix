#include <stdint.h>
#include "../kernel/kernel.h"

#define DEFAULT_LAPIC_ADDRESS 0xFEE00000

uint64_t local_apic_address = 0;
uint64_t io_apic_address = 0;

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uintptr_t get_lapic_base(void) {
    if (local_apic_address) return (uintptr_t)local_apic_address;
    uint64_t msr = read_msr(0x1B);
    uintptr_t addr = (uintptr_t)(msr & 0xFFFFF000ULL);
    return addr ? addr : DEFAULT_LAPIC_ADDRESS;
}

uint8_t lapic_get_id(void) {
    volatile uint32_t* lapic = (volatile uint32_t*)get_lapic_base();
    return (uint8_t)(lapic[0x20 / 4] >> 24);
}

void init_local_apic(void) {
    uint64_t apic_base_msr = read_msr(0x1B);
    apic_base_msr |= (1 << 11);
    write_msr(0x1B, apic_base_msr);

    uintptr_t lapic_addr = (uintptr_t)(apic_base_msr & 0xFFFFF000ULL);
    if (lapic_addr == 0) lapic_addr = DEFAULT_LAPIC_ADDRESS;
    if (local_apic_address == 0) local_apic_address = lapic_addr;

    volatile uint32_t* lapic = (volatile uint32_t*)get_lapic_base();
    lapic[0xF0 / 4] = (1 << 8) | 0xFF;
    lapic[0x80 / 4] = 0;
}

static inline void outb_apic(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void disable_pic(void) {
    outb_apic(0x21, 0xFF);
    outb_apic(0xA1, 0xFF);
}

void init_lapic_timer(uint32_t initial_count) {
    volatile uint32_t* lapic = (volatile uint32_t*)get_lapic_base();
    lapic[0x3E0 / 4] = 0x3;               // Divide by 16
    lapic[0x320 / 4] = (1 << 17) | 32;    // Periodic mode, vector 32
    lapic[0x380 / 4] = initial_count;
}

static void io_apic_write(uint32_t reg, uint32_t value) {
    uintptr_t base = io_apic_address ? (uintptr_t)io_apic_address : 0xFEC00000;
    volatile uint32_t* ioregsel = (volatile uint32_t*)(base + 0x00);
    volatile uint32_t* iowin    = (volatile uint32_t*)(base + 0x10);
    *ioregsel = reg;
    *iowin = value;
}

void io_apic_set_redirect(uint8_t irq, uint8_t vector) {
    uint32_t reg_low  = 0x10 + (irq * 2);
    uint32_t reg_high = 0x10 + (irq * 2) + 1;

    uint32_t low_value  = vector;
    uint32_t high_value = ((uint32_t)acpi_get_apic_id(0)) << 24;

    io_apic_write(reg_low, low_value);
    io_apic_write(reg_high, high_value);
}

void send_apic_eoi(void) {
    volatile uint32_t* lapic = (volatile uint32_t*)get_lapic_base();
    lapic[0xB0 / 4] = 0;
}

void init_ioapic(void) {
    io_apic_set_redirect(1, 33);
    io_apic_set_redirect(12, 0x2C);
}

void lapic_send_ipi(uint8_t apic_id, uint32_t icr_low) {
    volatile uint32_t* lapic = (volatile uint32_t*)get_lapic_base();
    while (lapic[0x300 / 4] & (1 << 12)) {
        __asm__ volatile("pause");
    }
    lapic[0x310 / 4] = ((uint32_t)apic_id) << 24;
    __asm__ volatile("mfence" ::: "memory");
    lapic[0x300 / 4] = icr_low;
    while (lapic[0x300 / 4] & (1 << 12)) {
        __asm__ volatile("pause");
    }
}

void lapic_send_init(uint8_t apic_id) {
    // Delivery mode = INIT (0b101 << 8 = 0x500), Level = Assert (1 << 14 = 0x4000)
    lapic_send_ipi(apic_id, 0x00004500);
}

void lapic_send_sipi(uint8_t apic_id, uint8_t vector) {
    // Delivery mode = StartUp (0b110 << 8 = 0x600), vector = physical page (0x08 -> 0x8000)
    lapic_send_ipi(apic_id, 0x00004600 | vector);
}

uint64_t timer_handler_c(uint64_t current_rsp) {
    uint64_t next_rsp = sched_tick(current_rsp);
    send_apic_eoi();
    return next_rsp;
}
