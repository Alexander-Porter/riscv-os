#include "types.h"
#include "spinlock.h"
#include "include/riscv.h"
#include "proc.h"
#include "global_func.h"

void initlock(struct spinlock *lk, char *name)
{
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

void acquire(struct spinlock *lk)
{
    push_off();
    if (holding(lk))
    {
    printf("acquire panic: lock=%s ra=%p\n", lk->name ? lk->name : "(null)", __builtin_return_address(0));
        panic("acquire");
    }

    while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;

    __sync_synchronize();
    lk->cpu = mycpu();
}

void release(struct spinlock *lk)
{
    if (!holding(lk)) {
        struct cpu *c = mycpu();
        printf("release panic: lock=%s locked=%d lk.cpu=%p curcpu=%p ra=%p\n",
               lk->name ? lk->name : "(null)", lk->locked, lk->cpu, c,
               __builtin_return_address(0));
        panic("release");
    }

    lk->cpu = 0;
    __sync_synchronize();
    __sync_lock_release(&lk->locked);

    pop_off();
}

int holding(struct spinlock *lk)
{
    return lk->locked && lk->cpu == mycpu();
}

void push_off(void)
{
    int old = intr_get();
    intr_off();

    struct cpu *c = mycpu();
    if (c->noff == 0)
        c->intena = old;
    c->noff++;
}

void pop_off(void)
{
    struct cpu *c = mycpu();
    if (intr_get())
        panic("pop_off - interruptible");
    if (c->noff < 1)
        panic("pop_off");

    c->noff--;
    if (c->noff == 0 && c->intena)
        intr_on();
}
