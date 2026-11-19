#pragma once
#include <stdint.h>

#define KERNEL_CS  0x08
#define KERNEL_DS  0x10
#define USER_DS    (0x18 | 3)   /* 0x1B */
#define USER_CS    (0x20 | 3)   /* 0x23 */
#define TSS_SEL    0x28

void init_gdt(void);
void init_gdt_cpu(int cpu_id);
void gdt_set_tss_rsp0(int cpu_id, uint64_t rsp0);
uint64_t gdt_get_cpu_stack_top(int cpu_id);
