#include "types.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

// 睡眠锁实现：结合自旋锁与睡眠，适合长耗时的临界区

void initsleeplock(struct sleeplock *lk, char *name)
{
    initlock(&lk->lk, "sleeplock");
    lk->name = name;
    lk->locked = 0;
    lk->pid = 0;
}

void acquiresleep(struct sleeplock *lk)
{
    acquire(&lk->lk);
    while (lk->locked)
    {
        struct proc *p = myproc();
        if (p == 0)
        {
            release(&lk->lk);
            while (lk->locked)
            {
                __sync_synchronize(); // 启动阶段直接忙等等待持有者释放
            }
            acquire(&lk->lk);
            continue;
        }
        sleep(lk, &lk->lk); // 持有自旋锁睡眠，避免忙等
    }
    lk->locked = 1;
    struct proc *p = myproc();
    lk->pid = p ? p->pid : -1; // 启动阶段使用特殊标记
    release(&lk->lk);
}

void releasesleep(struct sleeplock *lk)
{
    acquire(&lk->lk);
    lk->locked = 0;
    lk->pid = 0;
    wakeup(lk); // 唤醒等待该睡眠锁的进程
    release(&lk->lk);
}

int holdingsleep(struct sleeplock *lk)
{
    int r;
    acquire(&lk->lk);
    struct proc *p = myproc();
    if (p)
        r = lk->locked && (lk->pid == p->pid);
    else
        r = lk->locked && (lk->pid == -1);
    release(&lk->lk);
    return r;
}
