#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H

#include "types.h"
#include "spinlock.h"

// 内核态计数信号量，实现基于 sleep/wakeup 的阻塞机制
struct semaphore
{
    struct spinlock lock; // 保护 value 等字段
    int value;            // 当前计数值
    int active;           // 是否启用，0 表示未使用
};

void semaphore_system_init(void);
int semaphore_create(int initial);
int semaphore_acquire(int semid);
int semaphore_release(int semid);

#endif
