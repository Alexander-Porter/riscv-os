#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "include/riscv.h"
#include "proc.h"
#include "paging.h"
#include "global_func.h"
#include "syscall.h"

static int fetchstr(uint64 addr, char *buf, int max)
{
    struct proc *p = myproc();
    if (copyinstr(p->pagetable, buf, addr, max) < 0)
        return -1;
    return strlen(buf);
}

static uint64 argraw(int n)
{
    struct proc *p = myproc();
    switch (n)
    {
    case 0:
        return p->trapframe->a0;
    case 1:
        return p->trapframe->a1;
    case 2:
        return p->trapframe->a2;
    case 3:
        return p->trapframe->a3;
    case 4:
        return p->trapframe->a4;
    case 5:
        return p->trapframe->a5;
    default:
        panic("argraw");
        return -1;
    }
}

int argint(int n, int *ip)
{
    *ip = (int)argraw(n);
    return 0;
}

int argaddr(int n, uint64 *ip)
{
    *ip = argraw(n);
    return 0;
}

int argstr(int n, char *buf, int max)
{
    uint64 addr;
    if (argaddr(n, &addr) < 0)
        return -1;
    return fetchstr(addr, buf, max);
}

extern uint64 sys_exit(void);
extern uint64 sys_getpid(void);
extern uint64 sys_fork(void);
extern uint64 sys_wait(void);
extern uint64 sys_exec(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_kill(void);
extern uint64 sys_write(void);
extern uint64 sys_yield(void);
extern uint64 sys_setpriority(void);
extern uint64 sys_getpriority(void);
extern uint64 sys_getrunticks(void);
extern uint64 sys_sem_create(void);
extern uint64 sys_sem_wait(void);
extern uint64 sys_sem_post(void);
extern uint64 sys_rdtime(void);
extern uint64 sys_uptime(void);

static uint64 (*syscalls[])(void) = {
    [SYS_exit] = sys_exit,
    [SYS_getpid] = sys_getpid,
    [SYS_fork] = sys_fork,
    [SYS_wait] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_sbrk] = sys_sbrk,
    [SYS_sleep] = sys_sleep,
    [SYS_kill] = sys_kill,
    [SYS_write] = sys_write,
    [SYS_yield] = sys_yield,
    [SYS_setpriority] = sys_setpriority,
    [SYS_getpriority] = sys_getpriority,
    [SYS_getrunticks] = sys_getrunticks,
    [SYS_sem_create] = sys_sem_create,
    [SYS_sem_wait] = sys_sem_wait,
    [SYS_sem_post] = sys_sem_post,
    [SYS_rdtime] = sys_rdtime,
    [SYS_uptime] = sys_uptime,
};

void syscall(void)
{
    struct proc *p = myproc();
    int num = p->trapframe->a7;

    if (num > 0 && num < (int)(sizeof(syscalls) / sizeof(syscalls[0])) && syscalls[num])
    {
        uint64 ret = syscalls[num]();
        p->trapframe->a0 = ret;
    }
    else
    {
        printf("pid %d: unknown syscall %d\n", p->pid, num);
        p->trapframe->a0 = -1;
    }
}
