#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "include/riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "paging.h"
#include "global_func.h"
#include "exec.h"

#define MAX_PRIORITY 3
#define BASE_TIMESLICE 4
#define PRIORITY_MIN 0
#define PRIORITY_MAX (MAX_PRIORITY - 1)

extern volatile uint64 ticks;

extern char trampoline[];            // trampoline.S 中的向量入口
extern void forkret(void);           // swtch 返回后的入口
extern void usertrapret(void);       // 切回用户态
extern void swtch(struct context *, struct context *);

static void freeproc(struct proc *p);
static struct proc *allocproc(void);
static void wakeup_locked(struct proc *p);
static int allocpid(void);
static int timeslice_for_priority(int priority);
static void promote_waiting_process(struct proc *p);

#define PROC_DEBUG_INTERVAL 5

static int proc_created_total = 0;

static const char *proc_state_name(enum procstate state)
{
    switch (state)
    {
    case UNUSED:
        return "UNUSED";
    case USED:
        return "USED";
    case SLEEPING:
        return "SLEEPING";
    case RUNNABLE:
        return "RUNNABLE";
    case RUNNING:
        return "RUNNING";
    case ZOMBIE:
        return "ZOMBIE";
    default:
        return "UNKNOWN";
    }
}

static void debug_proc_table(void)
{
    printf("debug_proc_table: total_created=%d\n", proc_created_total);
    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
        acquire(&p->lock);
        enum procstate state = p->state;
        int pid = p->pid;
        int priority = p->priority;
        uint64 runtime = p->run_ticks;
        char name[sizeof(p->name)];
        if (state != UNUSED)
        {
            memmove(name, p->name, sizeof(name));
            release(&p->lock);
            name[sizeof(name) - 1] = '\0';
            printf("  pid=%d state=%s priority=%d runticks=%lu name=%s\n",
                   pid, proc_state_name(state), priority, runtime, name);
        }
        else
        {
            release(&p->lock);
        }
    }
}

static void proc_note_creation(void)
{
    proc_created_total++;
    if (proc_created_total % PROC_DEBUG_INTERVAL == 0)
    {
        debug_proc_table();
    }
}

struct cpu cpus[NCPU];
struct proc proc[NPROC];
static struct proc *initproc;        // 第一个用户进程
static struct spinlock pid_lock;     // 保护 nextpid 的锁
static struct spinlock wait_lock;    // wait()/exit() 协调锁
static int nextpid = 1;

static int allocpid(void)
{
    int pid;
    acquire(&pid_lock);
    pid = nextpid++;
    release(&pid_lock);
    return pid;
}

int cpuid(void)
{
    return (int)r_tp();
}

struct cpu *mycpu(void)
{
    int id = cpuid();
    return &cpus[id];
}

struct proc *myproc(void)
{
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

// 时间片计算: 高优先级(小数值)获得更长的时间片
// 设计理由: 高优先级任务更重要,应该获得更多CPU时间以便快速完成
// 优先级0(最高): BASE_TIMESLICE * 4 = 16 ticks ≈ 160ms
// 优先级1(中等): BASE_TIMESLICE * 2 = 8 ticks ≈ 80ms
// 优先级2(最低): BASE_TIMESLICE * 1 = 4 ticks ≈ 40ms
static int timeslice_for_priority(int priority) {
    // 边界检查
    if (priority < PRIORITY_MIN)
        priority = PRIORITY_MIN;
    if (priority > PRIORITY_MAX)
        priority = PRIORITY_MAX;
    
    // 反转优先级映射: 优先级越小(越重要),level越大,时间片越长
    int level = (MAX_PRIORITY - 1) - priority;
    return BASE_TIMESLICE << level;  // 等价于 BASE_TIMESLICE * 2^level
}

static void promote_waiting_process(struct proc *p)
{
    if (p->priority <= PRIORITY_MIN)
        return;

    uint64 wait_ticks = (ticks >= p->ready_time) ? (ticks - p->ready_time) : 0;
    uint64 threshold = (uint64)timeslice_for_priority(p->priority) * 2;
    if (wait_ticks >= threshold)
    {
        p->priority--;
        p->time_slice = 0;
        p->ready_time = ticks;
    }
}

void proc_mapstacks(pagetable_t kpgtbl)
{
    for (int i = 0; i < NPROC; i++)
    {
        void *pa = alloc_page();
        if (pa == 0)
            panic("proc_mapstacks: alloc_page");

        uint64 va = KSTACK(i);
        if (mappages(kpgtbl, va, PGSIZE, (uint64)pa, PTE_R | PTE_W) != 0)
            panic("proc_mapstacks: mappages");
    }
}

void procinit(void)
{
    initlock(&pid_lock, "nextpid");
    initlock(&wait_lock, "wait_lock");
    for (int i = 0; i < NPROC; i++)
    {
        struct proc *p = &proc[i];
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        p->kstack = KSTACK(i);
        p->priority = PRIORITY_MIN;
        p->time_slice = 0;
        p->ready_time = 0;
        p->run_ticks = 0;
    }
}

static void freeproc(struct proc *p)
{
    if (p->trapframe)
    {
        free_page(p->trapframe);
        p->trapframe = 0;
    }

    if (p->pagetable)
    {
        proc_freepagetable(p->pagetable, p->sz);
        p->pagetable = 0;
    }

    p->sz = 0;
    p->pid = 0;
    p->parent = 0;
    p->name[0] = '\0';
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->priority = PRIORITY_MIN;
    p->time_slice = 0;
    p->ready_time = 0;
    p->run_ticks = 0;
    p->state = UNUSED;
}

static struct proc *allocproc(void)
{
    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->state == UNUSED)
        {
            p->pid = allocpid();
            p->state = USED;

            p->trapframe = alloc_page();
            if (p->trapframe == 0)
            {
                freeproc(p);
                release(&p->lock);
                return 0;
            }
            memset(p->trapframe, 0, PGSIZE);

            p->pagetable = proc_pagetable(p);
            if (p->pagetable == 0)
            {
                freeproc(p);
                release(&p->lock);
                return 0;
            }

            memset(&p->context, 0, sizeof(p->context));
            p->context.ra = (uint64)forkret;
            p->context.sp = p->kstack + PGSIZE;
            p->priority = PRIORITY_MIN;
            p->time_slice = 0;
            p->ready_time = ticks;
            p->run_ticks = 0;
            return p;
        }
        release(&p->lock);
    }
    return 0;
}

pagetable_t proc_pagetable(struct proc *p)
{
    pagetable_t pagetable = uvmcreate(); // 创建空页表
    if (pagetable == 0)
        return 0;

    // 映射内核的TRAMPOLINE代码段
    if (mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R | PTE_X) < 0)
    {
        uvmfree(pagetable, 0);
        return 0;
    }

    // 映射进程的陷阱帧
    if (mappages(pagetable, TRAPFRAME, PGSIZE, (uint64)p->trapframe, PTE_R | PTE_W) < 0)
    {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0); // 清理已映射的TRAMPOLINE
        uvmfree(pagetable, 0);
        return 0;
    }

    return pagetable;
}

void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, sz);
}

void userinit(void)
{
    struct proc *p = allocproc();
    if (p == 0)
        panic("userinit: allocproc");

    initproc = p;
    char *argv[] = {"init", 0};
    if (exec_program_for_proc(p, "init", argv, 1) < 0)
        panic("userinit: exec init failed");

    p->priority = PRIORITY_MIN;
    p->time_slice = 0;
    p->ready_time = ticks;
    p->state = RUNNABLE;
    release(&p->lock);

    proc_note_creation();
}

int growproc(int n)
{
    struct proc *p = myproc();
    uint64 sz = p->sz;

    if (n > 0)
    {
#if ENABLE_LAZY_SBRK
        // Lazy allocation: 仅调整进程大小，实际物理页在缺页时按需分配
        sz = sz + (uint64)n;
#else
        // Eager allocation: 立即分配并映射物理页，行为与原实现一致
        sz = uvmalloc(p->pagetable, sz, sz + n);
        if (sz == 0)
            return -1;
#endif
    }
    else if (n < 0)
    {
        sz = uvmdealloc(p->pagetable, sz, sz + n);
    }
    p->sz = sz;
    return 0;
}

int fork(void)
{
    struct proc *p = myproc();
    struct proc *np = allocproc();
    if (np == 0)
        return -1;

    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
    {
        freeproc(np);
        release(&np->lock);
        return -1;
    }
    np->sz = p->sz;

    *(np->trapframe) = *(p->trapframe);
    np->trapframe->a0 = 0;  // 子进程返回 0

    np->parent = p;
    safestrcpy(np->name, p->name, sizeof(np->name));

    np->priority = p->priority;
    np->time_slice = 0;
    np->ready_time = ticks;
    np->run_ticks = 0;
    np->state = RUNNABLE;
    release(&np->lock);

    proc_note_creation();

    return np->pid;
}

void exit(int status)
{
    struct proc *p = myproc();

    if (p == initproc)
        panic("init exiting");

    acquire(&wait_lock);

    for (struct proc *pp = proc; pp < &proc[NPROC]; pp++)
    {
        if (pp->parent == p)
        {
            pp->parent = initproc;
            if (pp->state == ZOMBIE)
                wakeup_locked(initproc);
        }
    }

    wakeup_locked(p->parent);

    acquire(&p->lock);
    p->xstate = status;
    p->state = ZOMBIE;
    release(&wait_lock);

    sched();
    panic("zombie exit");
}

int wait(int *status)
{
    struct proc *p = myproc();  // 获取当前进程
    int havekids;

    acquire(&wait_lock);  // 获取等待锁，保护进程列表

    for (;;)
    {
        havekids = 0;
        for (struct proc *pp = proc; pp < &proc[NPROC]; pp++)  // 遍历所有进程
        {
            if (pp->parent == p)  // 检查是否为当前进程的子进程
            {
                havekids = 1;
                acquire(&pp->lock);  // 获取子进程锁
                if (pp->state == ZOMBIE)  // 子进程已结束但未回收
                {
                    int pid = pp->pid;
                    if (status != 0 && copyout(p->pagetable, (uint64)status, &pp->xstate, sizeof(pp->xstate)) < 0)
                    {  // 将退出状态复制到用户空间，如果失败则返回错误
                        release(&pp->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    freeproc(pp);  // 释放子进程资源
                    release(&pp->lock);
                    release(&wait_lock);
                    return pid;
                }
                release(&pp->lock);
            }
        }

        if (!havekids || p->killed)  // 无子进程或当前进程被杀死
        {
            release(&wait_lock);
            return -1;
        }

        sleep(p, &wait_lock);  // 睡眠等待子进程结束，释放锁
    }
}

int setpriority(int priority)
{
    if (priority < PRIORITY_MIN)
        priority = PRIORITY_MIN;
    if (priority > PRIORITY_MAX)
        priority = PRIORITY_MAX;

    struct proc *p = myproc();
    acquire(&p->lock);
    p->priority = priority;
    p->time_slice = 0;
    p->ready_time = ticks;
    release(&p->lock);
    return priority;
}

int getpriority(void)
{
    struct proc *p = myproc();
    acquire(&p->lock);
    int prio = p->priority;
    release(&p->lock);
    return prio;
}

uint64 getrunticks(void)
{
    struct proc *p = myproc();
    acquire(&p->lock);
    uint64 ticks_used = p->run_ticks;
    release(&p->lock);
    return ticks_used;
}

int sched_should_yield(struct proc *p)
{
    if (p == 0 || p->state != RUNNING)
        return 0;

    p->run_ticks++;
    p->time_slice++;

    if (p->time_slice >= timeslice_for_priority(p->priority))
    {
        p->time_slice = 0;
        p->ready_time = ticks;
        return 1;
    }

    return 0;
}

void scheduler(void)
{
    struct cpu *c = mycpu();
    c->proc = 0;

    for (;;)
    {
        intr_on();
        struct proc *selected = 0;

        for (struct proc *p = proc; p < &proc[NPROC]; p++)
        {
            acquire(&p->lock);
            if (p->state == RUNNABLE)
            {
                promote_waiting_process(p);

                if (selected == 0 || p->priority < selected->priority ||
                    (p->priority == selected->priority && p->ready_time <= selected->ready_time))
                {
                    if (selected)
                        release(&selected->lock);
                    selected = p;
                    continue;
                }
            }
            release(&p->lock);
        }

        if (selected)
        {
            selected->state = RUNNING;
            selected->time_slice = 0;
            c->proc = selected;
            swtch(&c->context, &selected->context);
            c->proc = 0;
            release(&selected->lock);
        }
    }
}

void sched(void)
{
    struct proc *p = myproc();

    if (!holding(&p->lock))
        panic("sched p->lock");
    if (mycpu()->noff != 1)
        panic("sched locks");
    if (p->state == RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    int intena = mycpu()->intena;
    swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

void yield(void)
{
    struct proc *p = myproc();
    acquire(&p->lock);
    p->time_slice = 0;
    p->ready_time = ticks;
    p->state = RUNNABLE;
    sched();
    release(&p->lock);
}

void sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    acquire(&p->lock);
    release(lk);

    p->chan = chan;
    p->state = SLEEPING;
    p->time_slice = 0;

    sched();

    p->chan = 0;
    release(&p->lock);
    acquire(lk);
}

static void wakeup_locked(struct proc *p)
{
    if (p == 0)
        return;

    acquire(&p->lock);
    if (p->state == SLEEPING)
    {
        p->state = RUNNABLE;
        p->time_slice = 0;
        p->ready_time = ticks;
        if (p->priority > PRIORITY_MIN)
            p->priority--;
    }
    release(&p->lock);
}

void wakeup(void *chan)
{
    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
        if (p == myproc())
            continue;

        acquire(&p->lock);
        if (p->state == SLEEPING && p->chan == chan)
        {
            p->state = RUNNABLE;
            p->time_slice = 0;
            p->ready_time = ticks;
            if (p->priority > PRIORITY_MIN)
                p->priority--;
        }
        release(&p->lock);
    }
}

int kill(int pid)
{
    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->pid == pid)
        {
            p->killed = 1;
            if (p->state == SLEEPING)
            {
                p->state = RUNNABLE;
                p->time_slice = 0;
                p->ready_time = ticks;
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    return -1;
}

void setkilled(struct proc *p)
{
    acquire(&p->lock);
    p->killed = 1;
    release(&p->lock);
}

int killed(struct proc *p)
{
    acquire(&p->lock);
    int k = p->killed;
    release(&p->lock);
    return k;
}

void forkret(void)
{
    static int first = 1;

    release(&myproc()->lock);

    if (first)
    {
        first = 0;
    }

    usertrapret();
}
