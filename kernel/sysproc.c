#include "types.h"
#include "param.h"
#include "include/riscv.h"
#include "proc.h"
#include "syscall.h"
#include "global_func.h"
#include "exec.h"
#include "semaphore.h"
#include "shm.h"

extern volatile uint64 ticks;

uint64 sys_exit(void)
{
    int status;
    if (argint(0, &status) < 0)
        return -1;
    exit(status);
    return 0; // not reached
}

uint64 sys_getpid(void)
{
    return (uint64)myproc()->pid;
}

uint64 sys_fork(void)
{
    return (uint64)fork();
}

uint64 sys_wait(void)
{
    uint64 addr;
    if (argaddr(0, &addr) < 0)
        return -1;
    return (uint64)wait((int *)addr);
}

uint64 sys_exec(void)
{
    uint64 path;
    uint64 argv;
    if (argaddr(0, &path) < 0 || argaddr(1, &argv) < 0)
        return (uint64)-1;
    return (uint64)do_exec(path, argv);
}

uint64 sys_sbrk(void)
{
    int n;
    if (argint(0, &n) < 0)
        return -1;
    struct proc *p = myproc();
    uint64 addr = p->sz;
    if (growproc(n) < 0)
        return (uint64)-1;
    return addr;
}

uint64 sys_sleep(void)
{
    int n;
    if (argint(0, &n) < 0)
        return -1;
    if (n < 0)
        n = 0;

    struct proc *p = myproc();
    uint64 start = ticks;
    while (ticks - start < (uint64)n)
    {
        if (killed(p))
            return -1;
        yield();
    }
    return 0;
}

uint64 sys_kill(void)
{
    int pid;
    if (argint(0, &pid) < 0)
        return -1;
    return (uint64)kill(pid);
}


uint64 sys_yield(void)
{
    yield();
    return 0;
}

uint64 sys_setpriority(void)
{
    int prio;
    if (argint(0, &prio) < 0)
        return -1;
    return (uint64)setpriority(prio);
}

uint64 sys_getpriority(void)
{
    return (uint64)getpriority();
}

uint64 sys_getrunticks(void)
{
    return getrunticks();
}

uint64 sys_rdtime(void)
{
    return r_time();
}

uint64 sys_uptime(void)
{
    return ticks;
}

uint64 sys_sem_create(void)
{
    int initial;
    if (argint(0, &initial) < 0)
        return (uint64)-1;
    return (uint64)semaphore_create(initial);
}

uint64 sys_sem_wait(void)
{
    int semid;
    if (argint(0, &semid) < 0)
        return (uint64)-1;
    return (uint64)semaphore_acquire(semid);
}

uint64 sys_sem_post(void)
{
    int semid;
    if (argint(0, &semid) < 0)
        return (uint64)-1;
    return (uint64)semaphore_release(semid);
}

uint64 sys_shm_create(void)
{
    return (uint64)shm_create();
}

uint64 sys_shm_get(void)
{
    int shmid;
    if (argint(0, &shmid) < 0)
        return (uint64)-1;
    uint64 va = 0;
    if (shm_map_for_proc(myproc(), shmid, &va) < 0)
        return (uint64)-1;
    return va;
}

uint64 sys_shm_unmap(void)
{
    uint64 addr;
    if (argaddr(0, &addr) < 0)
        return (uint64)-1;
    if (shm_unmap_for_proc(myproc(), addr) < 0)
        return (uint64)-1;
    return 0;
}
