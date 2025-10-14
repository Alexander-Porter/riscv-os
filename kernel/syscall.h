#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "types.h"

// 系统调用号，参考 xv6 的定义，保留常见子集便于扩展
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup     10
#define SYS_getpid  11
#define SYS_sbrk    12
#define SYS_sleep   13
#define SYS_uptime  14
#define SYS_open    15
#define SYS_write   16
#define SYS_mknod   17
#define SYS_unlink  18
#define SYS_link    19
#define SYS_mkdir   20
#define SYS_close   21
#define SYS_yield   22
#define SYS_setpriority 23
#define SYS_getpriority 24
#define SYS_getrunticks 25
#define SYS_sem_create 26
#define SYS_sem_wait 27
#define SYS_sem_post 28
#define SYS_uptime 29
#define SYS_rdtime 29

#ifndef __ASSEMBLER__
void syscall(void);
int argint(int n, int *ip);
int argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);
#endif

#endif
