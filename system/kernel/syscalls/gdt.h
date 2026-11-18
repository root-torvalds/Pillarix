#pragma once
#include <stdint.h>

#define KERNEL_CS  0x08
#define KERNEL_DS  0x10
#define USER_DS    (0x18 | 3)   /* 0x1B */
#define USER_CS    (0x20 | 3)   /* 0x23 */
#define TSS_SEL    0x28

void init_gdt(void);   /* вызывать ДО init_idt() */
