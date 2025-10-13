#include "semaphore.h"
#include "param.h"
#include "proc.h"
#include "global_func.h"

#define MAX_SEMAPHORES 64

static struct semaphore sem_table[MAX_SEMAPHORES];
static struct spinlock sem_table_lock;

static struct semaphore *get_sem(int semid)
{
    if (semid < 0 || semid >= MAX_SEMAPHORES)
        return 0;
    struct semaphore *sem = &sem_table[semid];
    if (!sem->active)
        return 0;
    return sem;
}

void semaphore_system_init(void)
{
    initlock(&sem_table_lock, "semtable");
    for (int i = 0; i < MAX_SEMAPHORES; i++)
    {
        initlock(&sem_table[i].lock, "sem");
        sem_table[i].value = 0;
        sem_table[i].active = 0;
    }
}

int semaphore_create(int initial)
{
    if (initial < 0)
        return -1;

    acquire(&sem_table_lock);
    for (int i = 0; i < MAX_SEMAPHORES; i++)
    {
        if (!sem_table[i].active)
        {
            struct semaphore *sem = &sem_table[i];
            acquire(&sem->lock);
            sem->value = initial;
            sem->active = 1;
            release(&sem->lock);
            release(&sem_table_lock);
            return i;
        }
    }
    release(&sem_table_lock);
    return -1;
}

int semaphore_acquire(int semid)
{
    struct semaphore *sem = get_sem(semid);
    if (sem == 0)
        return -1;

    acquire(&sem->lock);
    while (sem->active && sem->value == 0)
    {
        sleep(sem, &sem->lock);
    }
    if (!sem->active)
    {
        release(&sem->lock);
        return -1;
    }
    sem->value--;
    release(&sem->lock);
    return 0;
}

int semaphore_release(int semid)
{
    struct semaphore *sem = get_sem(semid);
    if (sem == 0)
        return -1;

    acquire(&sem->lock);
    if (!sem->active)
    {
        release(&sem->lock);
        return -1;
    }
    sem->value++;
    wakeup(sem);
    release(&sem->lock);
    return 0;
}
