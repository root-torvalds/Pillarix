#include <stdint.h>
#include "../kernel/kernel.h"

// Стандартный базовый физический адрес Local APIC по умолчанию
#define DEFAULT_LAPIC_ADDRESS 0xFEE00000

// Чтение из системного регистра MSR (Model Specific Register)
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

// Запись в системный регистр MSR
static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// Функция инициализации Local APIC
void init_local_apic(void) {
    // Шаг 1: Читаем регистр IA32_APIC_BASE (адрес 0x1B)
    uint64_t apic_base_msr = read_msr(0x1B);

    // Включаем бит 11 (APIC Global Enable), чтобы активировать APIC в процессоре
    apic_base_msr |= (1 << 11);
    write_msr(0x1B, apic_base_msr);

    // Шаг 2: Получаем физический адрес Local APIC (обнуляем младшие биты флагов)
    uintptr_t lapic_addr = (uintptr_t)(apic_base_msr & 0xFFFFF000);
    if (lapic_addr == 0) {
        lapic_addr = DEFAULT_LAPIC_ADDRESS; // Резервный стандартный адрес
    }

    // Шаг 3: Создаем указатель на память контроллера (volatile критически важен для железа!)
    // В Local APIC регистры расположены с шагом в 4 байта (uint32_t)
    volatile uint32_t* lapic = (volatile uint32_t*)lapic_addr;

    // Шаг 4: Настраиваем Spurious Interrupt Vector Register (SIVR) по смещению 0xF0
    // Бит 8 (1 << 8) — программное включение Local APIC (Software Enable)
    // 0xFF — номер вектора для ложных (spurious) прерываний
    lapic[0xF0 / 4] = (1 << 8) | 0xFF;

    // Шаг 5: Сбрасываем Task Priority Register (TPR) по смещению 0x80 в 0,
    // чтобы процессор не игнорировал никакие прерывания по приоритету
    lapic[0x80 / 4] = 0;
}

// Глобальные переменные для хранения физических адресов контроллеров
uint64_t local_apic_address = 0;
uint64_t io_apic_address = 0;

// Функция отправки байта в порт (ввод-вывод x86)
static inline void outb_apic(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Шаг 1: Полное отключение устаревшего контроллера прерываний 8259 PIC
void disable_pic(void) {
    // Маскируем все прерывания на ведущем и ведомом PIC
    outb_apic(0x21, 0xFF);
    outb_apic(0xA1, 0xFF);
}

// Шаг 2: Расширенная инициализация APIC на основе найденных данных MADT
void init_apic_hardware(BootInfo* info) {
    // Сначала отключаем старый PIC, чтобы он нам не мешал
    disable_pic();

    // Здесь во время обхода MADT мы сохраняем найденные адреса:
    // local_apic_address = madt->lica; (или извлекаем из MADT)
    // io_apic_address = io_apic->io_apic_address;

    (void)info;
}

// Внешняя переменная или функция для получения базового адреса LAPIC
// (можно использовать ту же логику, что мы писали ранее через MSR 0x1B)
extern uint64_t local_apic_address;

// Функция инициализации и запуска таймера Local APIC в периодическом режиме
void init_lapic_timer(uint32_t initial_count) {
    // Получаем базовый адрес Local APIC (стандартный или сохраненный)
    uintptr_t lapic_addr = local_apic_address ? (uintptr_t)local_apic_address : 0xFEE00000;
    volatile uint32_t* lapic = (volatile uint32_t*)lapic_addr;

    // Шаг 1: Настраиваем делитель частоты (Divide Configuration Register = 0x3E0)
    // Значение 0x3 устанавливает коэффициент деления на 16
    lapic[0x3E0 / 4] = 0x3;

    // Шаг 2: Настраиваем LVT Timer Register (смещение 0x320)
    // Бит 17 (1 << 17) включает периодический режим (Periodic Mode)
    // 32 — это номер вектора прерывания в IDT (например, IRQ0 переедет сюда)
    uint32_t lvt_timer_value = (1 << 17) | 32;
    lapic[0x320 / 4] = lvt_timer_value;

    // Шаг 3: Устанавливаем начальное значение счетчика (Initial Count Register = 0x380)
    // Чем больше число, тем реже будут срабатывать прерывания.
    // Для теста можно начать с примерного значения, например, 10000000.
    lapic[0x380 / 4] = initial_count;
}

// Внешняя переменная с физическим адресом I/O APIC, найденным при парсинге ACPI MADT
extern uint64_t io_apic_address;

// Низкоуровневая функция записи в регистры I/O APIC
static void io_apic_write(uint32_t reg, uint32_t value) {
    uintptr_t base = io_apic_address ? (uintptr_t)io_apic_address : 0xFEC00000;

    // Указатель на регистр выбора (IOREGSEL)
    volatile uint32_t* ioregsel = (volatile uint32_t*)(base + 0x00);
    // Указатель на окно данных (IOWIN)
    volatile uint32_t* iowin = (volatile uint32_t*)(base + 0x10);

    // Сначала сообщаем, с каким внутренним регистром работаем
    *ioregsel = reg;
    // Затем записываем туда значение
    *iowin = value;
}

// Функция настройки перенаправления прерывания (Redirection Table)
void io_apic_set_redirect(uint8_t irq, uint8_t vector) {
    // Таблица перенаправления для IRQ начинается с индекса 0x10.
    // Каждое IRQ занимает два регистра: младший (настройки) и старший (целевой процессор).
    uint32_t reg_low = 0x10 + (irq * 2);
    uint32_t reg_high = 0x10 + (irq * 2) + 1;

    // Настраиваем младшую часть (reg_low):
    // - vector: номер прерывания в нашей таблице IDT (например, 33 для клавиатуры)
    // - delivery mode: 0 (Fixed — обычная доставка прерывания)
    // - destination mode: 0 (Physical — отправка конкретному ядру)
    // - pin polarity: 0 (High active)
    // - trigger mode: 0 (Edge triggered — срабатывание по фронту)
    // - mask: 0 (Бит 16 равен 0, то есть прерывание РАЗРЕШЕНО и не замаскировано)
    uint32_t low_value = vector;

    // Настраиваем старшую часть (reg_high):
    // - содержит APIC ID ядра, на которое прилетит прерывание (0 для главного ядра)
    uint32_t high_value = 0;

    io_apic_write(reg_low, low_value);
    io_apic_write(reg_high, high_value);
}

extern uint64_t local_apic_address;

// Функция отправки сигнала EOI (End of Interrupt) в Local APIC
void send_apic_eoi(void) {
    uintptr_t lapic_addr = local_apic_address ? (uintptr_t)local_apic_address : 0xFEE00000;
    volatile uint32_t* lapic = (volatile uint32_t*)lapic_addr;
    // Регистр EOI расположен по смещению 0xB0 в Local APIC
    lapic[0xB0 / 4] = 0;
}

void init_ioapic(void) {
    // 1. Перенаправляем прерывание клавиатуры (IRQ 1) на вектор 33 в IDT
    io_apic_set_redirect(1, 33);

    // 2. Перенаправляем прерывание мыши (IRQ 12) на вектор 0x2C (44) в IDT
    io_apic_set_redirect(12, 0x2C);
}

extern void send_apic_eoi(void);

// Функция-обработчик прерывания таймера Local APIC на языке C
void timer_handler_c(void) {
    // Пока что обработчик ничего не делает, кроме отправки сигнала EOI,
    // чтобы процессор мог принимать следующие прерывания таймера.
    send_apic_eoi();
}
