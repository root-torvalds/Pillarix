#include <stdint.h>
#include <stdbool.h>
#include "../../kernel/kernel/kernel.h"

#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_READ_DMA_EXT  0x25
#define FIS_TYPE_REG_H2D      0x27

uint64_t ahci_abar_address;

typedef struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t reserved0;
    uint32_t tfd;
    uint32_t visa;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
} __attribute__((packed)) AHCI_Port;

typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t bcl;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  reserved[208];
    AHCI_Port ports[32];
} __attribute__((packed)) AHCI_MemoryMap;

typedef struct {
    uint8_t  cfl:5;
    uint8_t  a:1;
    uint8_t  w:1;
    uint8_t  p:1;
    uint8_t  r:1;
    uint8_t  b:1;
    uint8_t  c:1;
    uint8_t  rsv0:1;
    uint8_t  pmp:4;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed)) AHCI_CmdHeader;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc:22;
    uint32_t reserved1:9;
    uint32_t ioc:1;
} __attribute__((packed)) AHCI_PrdtEntry;

typedef struct {
    uint8_t        cfis[64];
    uint8_t        acmd[16];
    uint8_t        rsv[48];
    AHCI_PrdtEntry prdt_entry;
} __attribute__((packed)) AHCI_CommandTable;

__attribute__((aligned(1024))) AHCI_CmdHeader my_cmd_list[32];
__attribute__((aligned(256)))  uint8_t         my_received_fis[256];
__attribute__((aligned(1024))) AHCI_CommandTable my_command_table[32];

volatile AHCI_MemoryMap* ahci = 0;

static inline uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | (((uint32_t)slot) << 11) |
    (((uint32_t)func) << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    __asm__ volatile("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xCF8));
    uint32_t tmp;
    __asm__ volatile("inl %1, %0" : "=a"(tmp) : "Nd"((uint16_t)0xCFC));
    return tmp;
}

void init_ahci(void) {
    printf("[AHCI] Starting init...", 30, 130, 255, 255, 255, 255);
    swapBuffers(0);

    if (ahci_abar_address == 0) {
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                uint32_t id = pci_read_dword(bus, slot, 0, 0);
                if (id == 0xFFFFFFFF) continue;
                uint32_t class_reg = pci_read_dword(bus, slot, 0, 0x08);
                uint8_t base_class = (class_reg >> 24) & 0xFF;
                uint8_t sub_class  = (class_reg >> 16) & 0xFF;

                if (base_class == 0x01 && sub_class == 0x06) {
                    uint32_t bar5 = pci_read_dword(bus, slot, 0, 0x24);
                    if (bar5 != 0 && !(bar5 & 1)) {
                        uint64_t bar = bar5 & 0xFFFFFFF0;
                        // Проверка на 64-битный BAR (биты 1-2 равны 2, то есть значение 4)
                        if ((bar5 & 0x6) == 4) {
                            uint32_t bar6 = pci_read_dword(bus, slot, 0, 0x28);
                            bar |= ((uint64_t)bar6 << 32);
                        }
                        ahci_abar_address = bar;

                        // Дамп найденного PCI устройства
                        printf("[AHCI] Found PCI SATA controller", 30, 150, 0, 255, 0, 255);
                        print_hex(bus, 250, 150);
                        print_hex(ahci_abar_address & 0xFFFFFFFF, 350, 150);
                        swapBuffers(0);
                        break;
                    }
                }
            }
            if (ahci_abar_address != 0) break;
        }
    }

    if (ahci_abar_address == 0) {
        printf("[AHCI] ERROR: ABAR address is 0!", 30, 170, 255, 0, 0, 255);
        swapBuffers(0);
        return;
    }

    ahci = (volatile AHCI_MemoryMap*)((uint64_t)ahci_abar_address);

    // Дамп регистра GHC (Global Host Control)
    printf("[AHCI] GHC before AE:", 30, 190, 255, 255, 255, 255);
    print_hex(ahci->ghc, 200, 190);

    // Включаем AHCI режим (AE = bit 31)
    ahci->ghc |= (1 << 31);

    printf("[AHCI] GHC after AE: ", 30, 210, 255, 255, 255, 255);
    print_hex(ahci->ghc, 200, 210);
    swapBuffers(0);

    // Проверяем порт 0 (PI - Ports Implemented)
    if (!(ahci->pi & 1)) {
        printf("[AHCI] Port 0 not implemented in PI!", 30, 230, 255, 0, 0, 255);
        swapBuffers(0);
        return;
    }

    volatile AHCI_Port* port = &ahci->ports[0];

    printf("[AHCI] Port 0 SSTS:", 30, 250, 255, 255, 255, 255);
    print_hex(port->ssts, 200, 250);
    swapBuffers(0);

    // Проверка статуса подключения устройства (Device Detection = 3)
    if ((port->ssts & 0x0F) != 3) {
        printf("[AHCI] WARNING: No device detected on Port 0 (SSTS != 3)", 30, 270, 255, 255, 0, 255);
        swapBuffers(0);
        // Не возвращаем ошибку сразу, дадим шанс инициализироваться или посмотрим в дамп
    }

    // Сброс командного движка (ST = 0)
    port->cmd &= ~(1 << 0);
    uint32_t timeout = 1000000;
    while ((port->cmd & (1 << 15)) && --timeout);
    if (timeout == 0) {
        printf("[AHCI] Timeout waiting for CR to clear!", 30, 290, 255, 0, 0, 255);
    }

    // Сброс приема FIS (FRE = 0)
    port->cmd &= ~(1 << 4);
    timeout = 1000000;
    while ((port->cmd & (1 << 14)) && --timeout);
    if (timeout == 0) {
        printf("[AHCI] Timeout waiting for FR to clear!", 30, 310, 255, 0, 0, 255);
    }

    // Установка физических адресов командного списка и FIS
    uint64_t clb_phys = (uint64_t)&my_cmd_list;
    port->clb = (uint32_t)clb_phys;
    port->clbu = (uint32_t)(clb_phys >> 32);

    uint64_t fb_phys = (uint64_t)&my_received_fis;
    port->fb = (uint32_t)fb_phys;
    port->fbu = (uint32_t)(fb_phys >> 32);

    for (int i = 0; i < 32; i++) {
        my_cmd_list[i].prdtl = 1;
        uint64_t ctba_phys = (uint64_t)&my_command_table[i];
        my_cmd_list[i].ctba = (uint32_t)ctba_phys;
        my_cmd_list[i].ctbau = (uint32_t)(ctba_phys >> 32);
    }

    port->serr = 0xFFFFFFFF; // Очистка ошибок

    // Включаем FRE и ST
    port->cmd |= (1 << 4);
    port->cmd |= (1 << 0);

    printf("[AHCI] Init completed successfully!", 30, 330, 0, 255, 0, 255);
    swapBuffers(0);
}

bool ahci_read_sector(uint64_t lba, void* target_buffer) {
    if (!ahci) return false;
    volatile AHCI_Port* port = &ahci->ports[0];
    int slot = 0;
    if (port->ci & (1 << slot)) return false;

    port->is = (uint32_t)-1; // Сброс прерываний порта перед чтением

    volatile AHCI_CmdHeader* cmd_hdr = &my_cmd_list[slot];
    cmd_hdr->cfl = 5;
    cmd_hdr->w = 0;
    cmd_hdr->prdtl = 1;

    volatile AHCI_CommandTable* cmd_tbl = &my_command_table[slot];
    for (int i = 0; i < 64; i++) cmd_tbl->cfis[i] = 0;

    uintptr_t buf_phys = (uintptr_t)target_buffer;
    cmd_tbl->prdt_entry.dba = (uint32_t)buf_phys;
    cmd_tbl->prdt_entry.dbau = (uint32_t)(buf_phys >> 32);
    cmd_tbl->prdt_entry.dbc = 511;
    cmd_tbl->prdt_entry.ioc = 0;

    cmd_tbl->cfis[0] = FIS_TYPE_REG_H2D;
    cmd_tbl->cfis[1] = 0x80;
    cmd_tbl->cfis[2] = ATA_CMD_READ_DMA_EXT;

    cmd_tbl->cfis[4] = (uint8_t)lba;
    cmd_tbl->cfis[5] = (uint8_t)(lba >> 8);
    cmd_tbl->cfis[6] = (uint8_t)(lba >> 16);
    cmd_tbl->cfis[7] = 1 << 6;
    cmd_tbl->cfis[8] = (uint8_t)(lba >> 24);
    cmd_tbl->cfis[9] = (uint8_t)(lba >> 32);
    cmd_tbl->cfis[10] = (uint8_t)(lba >> 40);

    cmd_tbl->cfis[12] = 1;
    cmd_tbl->cfis[13] = 0;

    uint32_t timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && --timeout);
    if (timeout == 0) return false;

    port->ci = (1 << slot);

    timeout = 5000000;
    while (--timeout) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return false; // Проверка Task File Error
    }

    if (timeout == 0 || (port->is & (1 << 30))) return false;

    return true;
}

bool ahci_write_sector(uint64_t lba, const void* source_buffer) {
    if (!ahci) return false;
    volatile AHCI_Port* port = &ahci->ports[0];
    int slot = 0;
    if (port->ci & (1 << slot)) return false;

    port->is = (uint32_t)-1;

    volatile AHCI_CmdHeader* cmd_hdr = &my_cmd_list[slot];
    cmd_hdr->cfl = 5;
    cmd_hdr->w = 1;
    cmd_hdr->prdtl = 1;

    volatile AHCI_CommandTable* cmd_tbl = &my_command_table[slot];
    for (int i = 0; i < 64; i++) cmd_tbl->cfis[i] = 0;

    uintptr_t buf_phys = (uintptr_t)source_buffer;
    cmd_tbl->prdt_entry.dba = (uint32_t)buf_phys;
    cmd_tbl->prdt_entry.dbau = (uint32_t)(buf_phys >> 32);
    cmd_tbl->prdt_entry.dbc = 511;
    cmd_tbl->prdt_entry.ioc = 0;

    cmd_tbl->cfis[0] = FIS_TYPE_REG_H2D;
    cmd_tbl->cfis[1] = 0x80;
    cmd_tbl->cfis[2] = ATA_CMD_WRITE_DMA_EXT;

    cmd_tbl->cfis[4] = (uint8_t)lba;
    cmd_tbl->cfis[5] = (uint8_t)(lba >> 8);
    cmd_tbl->cfis[6] = (uint8_t)(lba >> 16);
    cmd_tbl->cfis[7] = 1 << 6;
    cmd_tbl->cfis[8] = (uint8_t)(lba >> 24);
    cmd_tbl->cfis[9] = (uint8_t)(lba >> 32);
    cmd_tbl->cfis[10] = (uint8_t)(lba >> 40);

    cmd_tbl->cfis[12] = 1;
    cmd_tbl->cfis[13] = 0;

    uint32_t timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && --timeout);
    if (timeout == 0) return false;

    flush_cache_line((void*)source_buffer, 512);

    port->ci = (1 << slot);

    timeout = 5000000;
    while (--timeout) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return false;
    }

    if (timeout == 0 || (port->is & (1 << 30))) return false;
    return true;
}

void print_hex(uint32_t val, unsigned int x, unsigned int y) {
    char hex_str[11];
    hex_str[0] = '0';
    hex_str[1] = 'x';
    const char* hex_chars = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        hex_str[2 + i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    hex_str[10] = '\0';
    printf(hex_str, x, y, 255, 255, 0, 255);
}

void debug_ahci_status(void) {
    if (!ahci) {
        printf("AHCI Pointer is NULL!", 30, 150, 255, 0, 0, 255);
        return;
    }
    volatile AHCI_Port* port = &ahci->ports[0];
    printf("SSTS:", 30, 170, 255, 255, 255, 255);
    print_hex(port->ssts, 100, 170);
    printf("TFD :", 30, 190, 255, 255, 255, 255);
    print_hex(port->tfd, 100, 190);
    printf("SERR:", 30, 210, 255, 255, 255, 255);
    print_hex(port->serr, 100, 210);
}

void flush_cache_line(void* addr, uint32_t length) {
    uintptr_t state = (uintptr_t)addr;
    for (uintptr_t i = state; i < state + length; i += 64) {
        __asm__ volatile("clflush (%0)" : : "r"(i) : "memory");
    }
}
