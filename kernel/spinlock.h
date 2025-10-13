#ifndef __SPINLOCK_H
#define __SPINLOCK_H

#include "types.h"

struct cpu;

// 简单自旋锁结构，记录持有者 CPU 与名称方便调试
struct spinlock {
    uint locked;       // 锁是否被持有
    char *name;        // 锁名称
    struct cpu *cpu;   // 持有锁的 CPU
};

void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int holding(struct spinlock *lk);

void push_off(void);
void pop_off(void);

#endif
