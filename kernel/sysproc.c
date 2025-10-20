#include "types.h"
#include "param.h"
#include "include/riscv.h"
#include "proc.h"
#include "syscall.h"
#include "global_func.h"
#include "exec.h"
#include "semaphore.h"

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

uint64 sys_write(void)
{
    int fd;
    uint64 src;
    int n;
    if (argint(0, &fd) < 0 || argaddr(1, &src) < 0 || argint(2, &n) < 0)
        return -1;
    if (n < 0)
        return -1;
    if (fd != 1 && fd != 2)
        return -1;
    if (src == 0 && n > 0)
        return -1;

    struct proc *p = myproc();
    if (p == 0)
        return -1;
    
    int written = 0;
    char buf[128];
    while (written < n)
    {
        int chunk = n - written;
        if (chunk > (int)sizeof(buf))
            chunk = (int)sizeof(buf);
        if (copyin(p->pagetable, buf, src + written, chunk) < 0)
            return -1;
        for (int i = 0; i < chunk; i++)
            uart_putc(buf[i]);
        written += chunk;
    }
    return written;
}

// 以下文件相关系统调用目前未实现文件系统：
// 为了参数与 ABI 对齐，提供空实现以返回错误码 -1。
uint64 sys_open(void)
{
    // int open(const char *path, int mode);
    // 参数解析保持与 xv6 一致
    uint64 path; int mode;
    if (argaddr(0, &path) < 0 || argint(1, &mode) < 0)
        return (uint64)-1;
    return (uint64)-1;
}

uint64 sys_close(void)
{
    // int close(int fd);
    int fd;
    if (argint(0, &fd) < 0)
        return (uint64)-1;
    return (uint64)-1;
}

uint64 sys_read(void)
{
    // int read(int fd, void *buf, int n);
    int fd, n; uint64 dst;
    if (argint(0, &fd) < 0 || argaddr(1, &dst) < 0 || argint(2, &n) < 0)
        return (uint64)-1;
    return (uint64)-1;
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
