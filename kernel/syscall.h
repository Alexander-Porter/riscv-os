#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "types.h"

#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_getpid  11
#define SYS_sbrk    12
#define SYS_sleep   13
#define SYS_uptime  14
#define SYS_open    15
#define SYS_write   16
#define SYS_close   21
#define SYS_yield   22
#define SYS_setpriority 23
#define SYS_getpriority 24
#define SYS_getrunticks 25
#define SYS_sem_create 26
#define SYS_sem_wait 27
#define SYS_sem_post 28
// extended
#define SYS_rdtime  29
#define SYS_shm_create 30
#define SYS_shm_get 31
#define SYS_shm_unmap 32
#define SYS_dup     33
#define SYS_pipe    34
#define SYS_fstat   35
#define SYS_chdir   36
#define SYS_mkdir   37
#define SYS_unlink  38
#define SYS_link    39
#define SYS_debugfs 40
#define SYS_crash   41

#ifndef __ASSEMBLER__
void syscall(void);
int argint(int n, int *ip);
int argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);
#endif

#endif
