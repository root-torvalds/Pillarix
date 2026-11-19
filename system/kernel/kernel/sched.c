#include <stdint.h>
#include "kernel.h"

#define TASK_KSTACK_SIZE (16 * 1024)

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
} task_t;

static task_t tasks[MAX_TASKS];
static uint8_t task_kstacks[MAX_TASKS][TASK_KSTACK_SIZE] __attribute__((aligned(16)));

static int cpu_current_task[MAX_CPUS];     /* -1 = idle / kernel_main */
static uint64_t cpu_idle_rsp[MAX_CPUS];    /* сохранённый RSP idle-цикла (или kernel_main на CPU 0) */
static uint64_t cpu_ticks[MAX_CPUS];
static int sched_ready = 0;

static spinlock_t sched_lock = SPINLOCK_INIT;

static void copy_name(char* dst, const char* src) {
    int i = 0;
    if (src) {
        while (src[i] != '\0' && i < 31) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void kthread_exit_trampoline(void) {
    sched_exit_current(0);
    while (1) { __asm__ volatile("hlt"); }
}

void init_scheduler(void) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].id = i;
        tasks[i].state = TASK_FREE;
        tasks[i].kstack_top = (uint64_t)(task_kstacks[i] + TASK_KSTACK_SIZE);
    }
    for (int c = 0; c < MAX_CPUS; c++) {
        cpu_current_task[c] = -1;
        cpu_idle_rsp[c] = 0;
        cpu_ticks[c] = 0;
    }
    sched_ready = 1;
    spin_unlock_irqrestore(&sched_lock, flags);
}

static int count_tasks_on_cpu_locked(int cpu_id) {
    int cnt = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_RUNNABLE && tasks[i].cpu_id == cpu_id) {
            cnt++;
        }
    }
    return cnt;
}

// Выбирает свободное ядро, а если все заняты — ядро с минимальным числом задач
static int pick_least_loaded_cpu_locked(void) {
    int ncpus = smp_cpu_count();
    if (ncpus < 1) ncpus = 1;

    int best_cpu = 0;
    int best_load = count_tasks_on_cpu_locked(0);

    for (int c = 1; c < ncpus; c++) {
        if (!smp_is_cpu_online(c)) continue;
        int load = count_tasks_on_cpu_locked(c);
        if (load < best_load) {
            best_load = load;
            best_cpu = c;
        }
    }
    return best_cpu;
}

static int pick_next_task_locked(int cpu_id, int current_tid) {
    int start = (current_tid >= 0) ? (current_tid + 1) : 0;
    for (int offset = 0; offset < MAX_TASKS; offset++) {
        int tid = (start + offset) % MAX_TASKS;
        if (tasks[tid].state == TASK_RUNNABLE && tasks[tid].cpu_id == cpu_id) {
            return tid;
        }
    }
    return -1;
}

static uint64_t sched_switch_internal(uint64_t current_rsp, int is_timer_tick) {
    if (!sched_ready) return current_rsp;

    int cpu = smp_current_cpu();
    if (cpu < 0 || cpu >= MAX_CPUS) return current_rsp;

    uint64_t flags = spin_lock_irqsave(&sched_lock);

    if (is_timer_tick) {
        cpu_ticks[cpu]++;
    }

    int cur_tid = cpu_current_task[cpu];
    if (cur_tid >= 0 && cur_tid < MAX_TASKS) {
        tasks[cur_tid].rsp = current_rsp;
        if (is_timer_tick && tasks[cur_tid].state == TASK_RUNNABLE) {
            tasks[cur_tid].ticks_used++;
        }
    } else {
        cpu_idle_rsp[cpu] = current_rsp;
    }

    int next_tid = pick_next_task_locked(cpu, cur_tid);

    // Если это ядро 0 (где работает терминал), даём терминалу (idle) тоже
    // получать тики, если на CPU 0 крутится несколько задач
    if (cpu == 0 && cur_tid >= 0 && next_tid <= cur_tid && cpu_idle_rsp[0] != 0) {
        // Возвращаемся в kernel_main/терминал раз за круг round-robin
        if (next_tid == -1 || next_tid <= cur_tid) {
            cpu_current_task[cpu] = -1;
            gdt_set_tss_rsp0(cpu, gdt_get_cpu_stack_top(cpu));
            uint64_t ret = cpu_idle_rsp[cpu];
            spin_unlock_irqrestore(&sched_lock, flags);
            return ret;
        }
    }

    if (next_tid >= 0) {
        cpu_current_task[cpu] = next_tid;
        gdt_set_tss_rsp0(cpu, tasks[next_tid].kstack_top);
        uint64_t ret = tasks[next_tid].rsp;
        spin_unlock_irqrestore(&sched_lock, flags);
        return ret;
    }

    // Нет runnable-задач на этом ядре -> возврат в idle
    if (cur_tid >= 0 && cpu_idle_rsp[cpu] != 0) {
        cpu_current_task[cpu] = -1;
        gdt_set_tss_rsp0(cpu, gdt_get_cpu_stack_top(cpu));
        uint64_t ret = cpu_idle_rsp[cpu];
        spin_unlock_irqrestore(&sched_lock, flags);
        return ret;
    }

    spin_unlock_irqrestore(&sched_lock, flags);
    return current_rsp;
}

uint64_t sched_tick(uint64_t current_rsp) {
    return sched_switch_internal(current_rsp, 1);
}

uint64_t sched_yield_c(uint64_t current_rsp) {
    return sched_switch_internal(current_rsp, 0);
}

void sched_yield(void) {
    __asm__ volatile("int $0x81");
}

static int sched_create_task_internal(const char* name, int is_user,
                                      uint64_t entry, uint64_t arg_or_rsp,
                                      uint64_t mem_base, uint64_t mem_size) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);

    int tid = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE) {
            tid = i;
            break;
        }
    }
    if (tid < 0) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return -1;
    }

    task_t* t = &tasks[tid];
    t->id = tid;
    t->cpu_id = pick_least_loaded_cpu_locked();
    t->is_user = is_user;
    t->user_mem_base = mem_base;
    t->user_mem_size = mem_size;
    t->exit_code = 0;
    t->ticks_used = 0;
    copy_name(t->name, name);

    uint64_t ktop = (uint64_t)(task_kstacks[tid] + TASK_KSTACK_SIZE);
    t->kstack_top = ktop;

    uint64_t* sp = (uint64_t*)ktop;
    uint64_t target_rsp = arg_or_rsp;

    if (!is_user) {
        // Кладём адрес возврата на стек ядра, чтобы при выходе из функции поток корректно завершился
        *(--sp) = (uint64_t)kthread_exit_trampoline;
        target_rsp = (uint64_t)sp;
        sp = (uint64_t*)(ktop - 16); /* оставляем выравнивание для interrupt frame */
    }

    // Формируем кадр прерывания (совпадает с timer_handler_asm / sched_yield_asm)
    *(--sp) = is_user ? USER_DS : KERNEL_DS;  /* SS */
    *(--sp) = target_rsp;                     /* RSP */
    *(--sp) = 0x202ULL;                       /* RFLAGS: IF=1 */
    *(--sp) = is_user ? USER_CS : KERNEL_CS;  /* CS */
    *(--sp) = entry;                          /* RIP */

    *(--sp) = 0;                              /* rax */
    *(--sp) = 0;                              /* rbx */
    *(--sp) = is_user ? 0 : arg_or_rsp;       /* rcx (arg1 в ms_abi для kthread) */
    *(--sp) = 0;                              /* rdx */
    *(--sp) = 0;                              /* rsi */
    *(--sp) = 0;                              /* rdi */
    *(--sp) = 0;                              /* rbp */
    *(--sp) = 0;                              /* r8  */
    *(--sp) = 0;                              /* r9  */
    *(--sp) = 0;                              /* r10 */
    *(--sp) = 0;                              /* r11 */
    *(--sp) = 0;                              /* r12 */
    *(--sp) = 0;                              /* r13 */
    *(--sp) = 0;                              /* r14 */
    *(--sp) = 0;                              /* r15 */

    t->rsp = (uint64_t)sp;
    t->state = TASK_RUNNABLE;

    spin_unlock_irqrestore(&sched_lock, flags);
    return tid;
}

int sched_spawn_kthread(const char* name, void (*entry)(uint64_t arg), uint64_t arg) {
    return sched_create_task_internal(name, 0, (uint64_t)entry, arg, 0, 0);
}

int sched_spawn_user(const char* name, uint64_t entry, uint64_t user_rsp,
                     uint64_t mem_base, uint64_t mem_size) {
    return sched_create_task_internal(name, 1, entry, user_rsp, mem_base, mem_size);
}

uint64_t sched_wait(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return (uint64_t)-1;

    while (1) {
        uint64_t flags = spin_lock_irqsave(&sched_lock);
        if (tasks[task_id].state == TASK_ZOMBIE) {
            uint64_t code = tasks[task_id].exit_code;
            tasks[task_id].state = TASK_FREE;
            spin_unlock_irqrestore(&sched_lock, flags);
            return code;
        }
        if (tasks[task_id].state == TASK_FREE) {
            spin_unlock_irqrestore(&sched_lock, flags);
            return 0;
        }
        spin_unlock_irqrestore(&sched_lock, flags);
        __asm__ volatile("sti; hlt");
    }
}

void sched_exit_current(uint64_t code) {
    int cpu = smp_current_cpu();
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    int tid = (cpu >= 0 && cpu < MAX_CPUS) ? cpu_current_task[cpu] : -1;
    if (tid >= 0 && tid < MAX_TASKS) {
        tasks[tid].exit_code = code;
        tasks[tid].state = TASK_ZOMBIE;
    }
    spin_unlock_irqrestore(&sched_lock, flags);

    while (1) {
        sched_yield();
        __asm__ volatile("sti; hlt");
    }
}

int sched_current_user_bounds(uint64_t* base_out, uint64_t* size_out) {
    int cpu = smp_current_cpu();
    if (cpu < 0 || cpu >= MAX_CPUS) return 0;
    int tid = cpu_current_task[cpu];
    if (tid < 0 || tid >= MAX_TASKS) return 0;
    if (!tasks[tid].is_user) return 0;
    *base_out = tasks[tid].user_mem_base;
    *size_out = tasks[tid].user_mem_size;
    return 1;
}

uint64_t sched_get_total_ticks(void) {
    uint64_t sum = 0;
    int n = smp_cpu_count();
    for (int i = 0; i < n; i++) sum += cpu_ticks[i];
    return sum;
}

int sched_get_cpu_task_count(int cpu_id) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    int cnt = count_tasks_on_cpu_locked(cpu_id);
    spin_unlock_irqrestore(&sched_lock, flags);
    return cnt;
}

uint64_t sched_get_cpu_ticks(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) return 0;
    return cpu_ticks[cpu_id];
}

int sched_snapshot_tasks(task_info_t* out, int max_items) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    int n = 0;
    for (int i = 0; i < MAX_TASKS && n < max_items; i++) {
        if (tasks[i].state != TASK_FREE) {
            out[n].id = tasks[i].id;
            out[n].cpu_id = tasks[i].cpu_id;
            out[n].is_user = tasks[i].is_user;
            out[n].state = tasks[i].state;
            out[n].ticks_used = tasks[i].ticks_used;
            copy_name(out[n].name, tasks[i].name);
            n++;
        }
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    return n;
}
