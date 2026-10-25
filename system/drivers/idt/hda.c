#include <stdint.h>
#include "../../kernel/kernel/kernel.h"
// Регистровые смещения Intel HDA MMIO
#define HDA_GCTL        0x08
#define HDA_CORBLBASE   0x40
#define HDA_CORBUBASE   0x44
#define HDA_CORBWP      0x48
#define HDA_CORBRP      0x4A
#define HDA_CORBCTL     0x4C
#define HDA_RIRBLBASE   0x50
#define HDA_RIRBUBASE   0x54
#define HDA_RIRBWP      0x58
#define HDA_RIRBCTL     0x5C

// Буферы для команд (CORB) и ответов (RIRB)
static uint32_t corb_buffer[256] __attribute__((aligned(128)));
static uint64_t rirb_buffer[256] __attribute__((aligned(128)));

// Увеличенный размер буфера для стабильного звука (~1.5 секунды при 44100 Гц)
#define AUDIO_BUF_SAMPLES 65536
static int16_t pcm_audio_buffer[AUDIO_BUF_SAMPLES] __attribute__((aligned(128)));

// Структура элемента таблицы дескрипторов буфера (BDL Entry)
struct hda_bdl_entry {
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t length;
    uint32_t interrupt_on_completion;
} __attribute__((packed));

static struct hda_bdl_entry bdl_table[2] __attribute__((aligned(128)));

// Функции доступа к конфигурационному пространству PCI через порты 0xCF8 и 0xCFC
static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1U << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

static void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (1U << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

// Функция отправки верба (команды) аудиокодеку через CORB
uint32_t hda_send_verb(volatile uint32_t* hda_regs, uint8_t codec_addr, uint8_t node, uint32_t verb) {
    uint32_t cmd = ((uint32_t)codec_addr << 28) | ((uint32_t)node << 20) | verb;

    uint16_t wp = *(volatile uint16_t*)((uint8_t*)hda_regs + HDA_CORBWP);
    wp = (wp + 1) % 256;

    corb_buffer[wp] = cmd;

    *(volatile uint16_t*)((uint8_t*)hda_regs + HDA_CORBWP) = wp;
    return 0;
}

// Функция инициализации очередей CORB и RIRB для связи с кодеком
void hda_init_corb_rirb(volatile uint32_t* hda_regs) {
    hda_regs[HDA_CORBCTL / 4] = 0;
    while (hda_regs[HDA_CORBCTL / 4] & 2) { __asm__ volatile("nop"); }

    uint64_t corb_phys = (uint64_t)corb_buffer;
    hda_regs[HDA_CORBLBASE / 4] = (uint32_t)(corb_phys & 0xFFFFFFFF);
    hda_regs[HDA_CORBUBASE / 4] = (uint32_t)(corb_phys >> 32);

    *(volatile uint16_t*)((uint8_t*)hda_regs + HDA_CORBWP) = 0;
    *(volatile uint16_t*)((uint8_t*)hda_regs + HDA_CORBRP) = (1 << 15);
    for (volatile int i = 0; i < 1000; i++) { __asm__ volatile("nop"); }
    *(volatile uint16_t*)((uint8_t*)hda_regs + HDA_CORBRP) = 0;

    hda_regs[HDA_CORBCTL / 4] = 2;

    hda_regs[HDA_RIRBCTL / 4] = 0;
    uint64_t rirb_phys = (uint64_t)rirb_buffer;
    hda_regs[HDA_RIRBLBASE / 4] = (uint32_t)(rirb_phys & 0xFFFFFFFF);
    hda_regs[HDA_RIRBUBASE / 4] = (uint32_t)(rirb_phys >> 32);

    hda_regs[HDA_RIRBCTL / 4] = 2;
}

// Функция отправки настроек кодеку для разблокировки звука
void hda_init_codec(volatile uint32_t* hda_regs) {
    uint8_t codec_addr = 0;

    hda_send_verb(hda_regs, codec_addr, 0x02, 0x37F);
    hda_send_verb(hda_regs, codec_addr, 0x03, 0x37F);

    hda_send_verb(hda_regs, codec_addr, 0x0B, 0x707);
    hda_send_verb(hda_regs, codec_addr, 0x0B, 0x37F);
    hda_send_verb(hda_regs, codec_addr, 0x0B, 0x706);
}

// Функция генерации простого звукового сигнала (тона)
void hda_generate_tone(void) {
    for (int i = 0; i < AUDIO_BUF_SAMPLES; i++) {
        if ((i / 40) % 2 == 0) {
            pcm_audio_buffer[i] = 10000;
        } else {
            pcm_audio_buffer[i] = -10000;
        }
    }
}

// Функция инициализации и запуска воспроизведения звука
void hda_start_playback(volatile uint32_t* hda_regs) {
    hda_generate_tone();

    uint64_t buf_phys = (uint64_t)pcm_audio_buffer;
    bdl_table[0].addr_low = (uint32_t)(buf_phys & 0xFFFFFFFF);
    bdl_table[0].addr_high = (uint32_t)(buf_phys >> 32);
    bdl_table[0].length = sizeof(pcm_audio_buffer);
    bdl_table[0].interrupt_on_completion = 1;

    volatile uint32_t* stream_regs = (volatile uint32_t*)((uint8_t*)hda_regs + 0x80);

    stream_regs[0] &= ~1;
    while (stream_regs[0] & 1) { __asm__ volatile("nop"); }
    stream_regs[0] |= 1;
    for (volatile int i = 0; i < 1000; i++) { __asm__ volatile("nop"); }

    uint64_t bdl_phys = (uint64_t)bdl_table;
    stream_regs[0x18 / 4] = (uint32_t)(bdl_phys & 0xFFFFFFFF);
    stream_regs[0x1C / 4] = (uint32_t)(bdl_phys >> 32);

    stream_regs[0x08 / 4] = sizeof(pcm_audio_buffer);
    *(volatile uint16_t*)((uint8_t*)stream_regs + 0x0C) = 0;
    *(volatile uint16_t*)((uint8_t*)stream_regs + 0x12) = 0x0010;

    stream_regs[0] |= (1 << 1);
}

// Главная функция инициализации Intel High Definition Audio контроллера
void init_intel_hda(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t class_rev = pci_read32(bus, slot, func, 0x08);
                uint8_t base_class = (class_rev >> 24) & 0xFF;
                uint8_t sub_class  = (class_rev >> 16) & 0xFF;

                if (base_class == 0x04 && sub_class == 0x03) {
                    uint32_t bar0 = pci_read32(bus, slot, func, 0x10);
                    uint64_t hda_base = bar0 & 0xFFFFFFF0;

                    uint32_t cmd = pci_read32(bus, slot, func, 0x04);
                    cmd |= (1 << 1) | (1 << 2);
                    pci_write32(bus, slot, func, 0x04, cmd);

                    volatile uint32_t* hda_regs = (volatile uint32_t*)hda_base;

                    uint32_t gctl = hda_regs[0x08 / 4];
                    gctl &= ~1;
                    hda_regs[0x08 / 4] = gctl;

                    for (volatile int i = 0; i < 10000; i++) {
                        __asm__ volatile("nop");
                    }

                    gctl |= 1;
                    hda_regs[0x08 / 4] = gctl;

                    hda_init_corb_rirb(hda_regs);
                    hda_init_codec(hda_regs);
                    hda_start_playback(hda_regs);
                    return;
                }
            }
        }
    }
}
