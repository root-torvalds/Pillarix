#pragma once
#include <stdint.h>

void init_smp(void);
int smp_current_cpu(void);
int smp_cpu_count(void);
int smp_is_cpu_online(int cpu_id);
