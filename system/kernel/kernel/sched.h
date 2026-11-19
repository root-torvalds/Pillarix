#pragma once
#include <stdint.h>

#define MAX_TASKS 32

typedef enum {
    TASK_FREE = 0,
    TASK_RUNNABLE,
    TASK_ZOMBIE
} task_state_t;

typedef struct {
    int id;
    int cpu_id;
    int is_user;
    task_state_t state;
    uint64_t rsp;
    uint64_t kstack_top;
    uint64_t user_mem_base;
    uint64_t user_mem_size;
    uint64_t exit_code;
    uint64_t ticks_used;
    char name[32];
} task_info_t;

void init_scheduler(void);
uint64_t sched_tick(uint64_t current_rsp);
uint64_t sched_yield_c(uint64_t current_rsp);
void sched_yield(void);

int sched_spawn_kthread(const char* name, void (*entry)(uint64_t arg), uint64_t arg);
int sched_spawn_user(const char* name, uint64_t entry, uint64_t user_rsp,
                     uint64_t mem_base, uint64_t mem_size);
uint64_t sched_wait(int task_id);
void sched_exit_current(uint64_t code);

int sched_current_user_bounds(uint64_t* base_out, uint64_t* size_out);
uint64_t sched_get_total_ticks(void);
int sched_get_cpu_task_count(int cpu_id);
uint64_t sched_get_cpu_ticks(int cpu_id);
int sched_snapshot_tasks(task_info_t* out, int max_items);
