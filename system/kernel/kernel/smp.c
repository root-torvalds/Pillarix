#include <stdint.h>
#include "kernel.h"

#define TRAMPOLINE_ADDR 0x8000ULL

extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];
extern uint8_t ap_trampoline_data[];

struct __attribute__((packed)) ApTrampolineData {
    uint64_t cr3;
    uint64_t efer;
    uint64_t stack;
    uint64_t entry;
    uint32_t cpu_id;
};

static volatile int cpu_online[MAX_CPUS];
static int total_cpus_online = 1;

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_efer(void) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000080));
    return ((uint64_t)high << 32) | low;
}

static void smp_delay(uint32_t loops) {
    for (volatile uint32_t i = 0; i < loops; i++) {
        __asm__ volatile("pause");
    }
}

void ap_main(uint32_t cpu_id) {
    init_gdt_cpu((int)cpu_id);
    load_idt_ap();
    init_local_apic();
    init_lapic_timer(10000000);

    __asm__ volatile("mfence" ::: "memory");
    cpu_online[cpu_id] = 1;

    __asm__ volatile("sti");
    while (1) {
        __asm__ volatile("hlt");
    }
}

void init_smp(void) {
    for (int i = 0; i < MAX_CPUS; i++) cpu_online[i] = 0;
    cpu_online[0] = 1;
    total_cpus_online = 1;

    int count = acpi_get_cpu_count();
    if (count <= 1) return;

    uint64_t tramp_size = (uint64_t)(ap_trampoline_end - ap_trampoline_start);
    uint64_t data_off   = (uint64_t)(ap_trampoline_data - ap_trampoline_start);

    memcpy((void*)TRAMPOLINE_ADDR, ap_trampoline_start, (unsigned long)tramp_size);

    volatile struct ApTrampolineData* tdata =
        (volatile struct ApTrampolineData*)(TRAMPOLINE_ADDR + data_off);

    uint64_t bsp_cr3  = read_cr3();
    uint64_t bsp_efer = read_efer();

    for (int cpu = 1; cpu < count && cpu < MAX_CPUS; cpu++) {
        uint8_t apic_id = acpi_get_apic_id(cpu);

        tdata->cr3    = bsp_cr3;
        tdata->efer   = bsp_efer;
        tdata->stack  = gdt_get_cpu_stack_top(cpu);
        tdata->entry  = (uint64_t)ap_main;
        tdata->cpu_id = (uint32_t)cpu;
        __asm__ volatile("mfence" ::: "memory");

        // 1. INIT IPI
        lapic_send_init(apic_id);
        smp_delay(1000000);

        // 2. SIPI #1 (вектор 0x08 -> адрес 0x8000)
        lapic_send_sipi(apic_id, (uint8_t)(TRAMPOLINE_ADDR >> 12));
        smp_delay(200000);

        // 3. SIPI #2 (по стандарту Intel, если ещё не проснулось)
        if (!cpu_online[cpu]) {
            lapic_send_sipi(apic_id, (uint8_t)(TRAMPOLINE_ADDR >> 12));
        }

        // Ждём подтверждения запуска ядра
        for (volatile uint32_t w = 0; w < 5000000; w++) {
            if (cpu_online[cpu]) break;
            __asm__ volatile("pause");
        }

        if (cpu_online[cpu]) {
            total_cpus_online++;
        }
    }
}

int smp_current_cpu(void) {
    uint8_t aid = lapic_get_id();
    return acpi_find_cpu_by_apic_id(aid);
}

int smp_cpu_count(void) {
    return total_cpus_online;
}

int smp_is_cpu_online(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) return 0;
    return cpu_online[cpu_id];
}
