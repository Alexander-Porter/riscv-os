#ifndef __SLEEPLOCK_H
#define __SLEEPLOCK_H

#include "types.h"
#include "spinlock.h"

struct sleeplock {
    uint locked;          // 是否被持有
    struct spinlock lk;   // 自旋锁，保护本结构
    char *name;           // 锁名称
    int pid;              // 持锁进程的 PID
};

void initsleeplock(struct sleeplock *lk, char *name);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);
int holdingsleep(struct sleeplock *lk);

#endif
