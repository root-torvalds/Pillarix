#pragma once
#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline uint64_t irq_save_disable(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

static inline void spin_lock(spinlock_t* lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        while (lock->locked) {
            __asm__ volatile("pause" ::: "memory");
        }
    }
}

static inline void spin_unlock(spinlock_t* lock) {
    __sync_lock_release(&lock->locked);
}

static inline uint64_t spin_lock_irqsave(spinlock_t* lock) {
    uint64_t flags = irq_save_disable();
    spin_lock(lock);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t* lock, uint64_t flags) {
    spin_unlock(lock);
    irq_restore(flags);
}
