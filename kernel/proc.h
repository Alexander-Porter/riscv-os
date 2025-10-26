#ifndef __PROC_H
#define __PROC_H

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "paging.h"
#include "memlayout.h"

struct trapframe;
struct file;
struct inode;

struct context {
    uint64 ra;
    uint64 sp;
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

#define PROC_SHM_MAX SHM_MAX_PAGES

struct shm_mapping {
    uint64 va;   // 映射到用户空间的虚拟地址
    int shmid;   // 共享内存编号
    int used;    // 是否有效
};

struct cpu {
    struct proc *proc;      // 当前在 CPU 上运行的进程
    struct context context; // scheduler 的上下文
    int noff;               // push_off 嵌套深度
    int intena;             // push_off 前的中断状态
};

extern struct cpu cpus[NCPU];
extern struct cpu *mycpu(void);

enum procstate {
    UNUSED,
    USED,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE,
};

struct trapframe {
    uint64 kernel_satp;
    uint64 kernel_sp;
    uint64 kernel_trap;
    uint64 epc;
    uint64 kernel_hartid;
    uint64 ra;
    uint64 sp;
    uint64 gp;
    uint64 tp;
    uint64 t0;
    uint64 t1;
    uint64 t2;
    uint64 s0;
    uint64 s1;
    uint64 a0;
    uint64 a1;
    uint64 a2;
    uint64 a3;
    uint64 a4;
    uint64 a5;
    uint64 a6;
    uint64 a7;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
    uint64 t3;
    uint64 t4;
    uint64 t5;
    uint64 t6;
};

struct proc {
    struct spinlock lock;

    enum procstate state;
    void *chan;
    int killed;
    int xstate;
    int pid;

    struct proc *parent;

    uint64 kstack;
    uint64 sz;
    pagetable_t pagetable;
    struct trapframe *trapframe;
    struct context context;
    char name[16];
    int priority;          // 优先级，数值越小优先级越高
    int time_slice;        // 当前时间片已累计的tick数
    uint64 ready_time;     // 进入RUNNABLE状态的时间戳
    uint64 run_ticks;      // 历史运行tick统计
    struct shm_mapping shm_regions[PROC_SHM_MAX]; // 当前进程持有的共享页
    int shm_region_count;  // 共享页数量
    struct file *ofile[NOFILE]; // 打开文件表
    struct inode *cwd;          // 当前工作目录
};

extern struct proc proc[NPROC];

void procinit(void);
void proc_mapstacks(pagetable_t);
int cpuid(void);
struct proc *myproc(void);
pagetable_t proc_pagetable(struct proc *p);
void proc_freepagetable(pagetable_t, uint64);
void userinit(void);
int growproc(int);
int fork(void);
void exit(int);
int wait(int *);
void scheduler(void);
void sched(void);
void yield(void);
void sleep(void *, struct spinlock *);
void wakeup(void *);
int kill(int);
void setkilled(struct proc *p);
int killed(struct proc *p);
void forkret(void);
int sched_should_yield(struct proc *p);
int setpriority(int priority);
int getpriority(void);
uint64 getrunticks(void);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);

#endif
