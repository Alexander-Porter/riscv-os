// 进程管理 (proc.c)

#include "proc.h"
#include "global_func.h"
#include "memlayout.h"

#define NPROC 64 // 最大进程数

struct proc proc[NPROC];
struct proc *initproc;

struct cpu cpus[1];

int nextpid = 1;

void forkret(void);
extern void swtch(struct context*, struct context*);

extern char _initcode_start[], _initcode_end[];

// 初始化进程表
void
proc_init(void)
{
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++) {
      p->state = UNUSED;
  }
}

// 分配一个新进程
// 找到一个UNUSED的proc, 初始化它的状态为USED, 分配PID
// 并为其分配一个内核栈和trapframe
struct proc* alloc_proc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->state == UNUSED) {
      goto found;
    }
  }
  return 0; // 没找到

found:
  p->pid = nextpid++;
  p->state = USED;

  // 为进程分配trapframe页
  if((p->trapframe = (struct trapframe *)alloc_page()) == 0){
    p->state = UNUSED;
    return 0;
  }

  // 为进程分配内核栈
  if((p->kstack = (uint64)alloc_page()) == 0) {
    // free trapframe page
    free_page(p->trapframe);
    p->state = UNUSED;
    return 0;
  }

  // 初始化上下文, 让ra指向forkret, sp指向内核栈顶
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// forkret: 新进程的入口点
void forkret()
{
  // 我们还不能返回到用户空间，所以暂时什么都不做
  // 后续这里将调用usertrapret
}

// 一个非常简单的调度器。循环遍历进程表，
// 寻找一个可运行的(RUNNABLE)进程，然后切换到它。
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = &cpus[0];
  
  c->proc = 0; // 当前没有进程在运行
  for(;;){
    // 在RISC-V中, M模式下的中断是默认开启的。
    // S模式下的中断需要手动开启。
    // 我们将在trap.c中处理中断使能。

    // 遍历进程表
    for(p = proc; p < &proc[NPROC]; p++) {
      if(p->state == RUNNABLE) {
        // 找到了一个可运行的进程，准备切换
        p->state = RUNNING;
        c->proc = p;
        printf("scheduler: 进程 %d 开始运行\n", p->pid);
        // swtch是一个汇编函数, 它会保存当前上下文(调度器的上下文)
        // 到c->context, 然后恢复p->context指定的下一个进程的上下文
        // 从而实现进程切换。
        swtch(&c->context, &p->context);

        // 当进程切换回来时, 说明它已经执行了一段时间。
        // 进程应该在返回前改变自己的状态(例如, 变为RUNNABLE或SLEEPING)
        c->proc = 0;
      }
    }
  }
}

// 创建第一个用户进程
void
user_init(void)
{
  struct proc *p;

  p = alloc_proc();
  if(p == 0)
    panic("user_init: alloc_proc failed");
  
  initproc = p;
  
  // 为第一个进程创建页表
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0)
    panic("user_init: proc_pagetable failed");

  // 将initcode.S的代码加载到内存地址0。
  // initcode.S是第一个用户程序, 它非常简单, 只是执行一个ecall。
  // _initcode_start和_initcode_end是在kernel.ld中定义的符号,
  // 它们分别指向initcode.S的开始和结束地址。
  uvminit(p->pagetable, (uchar*)_initcode_start, _initcode_end - _initcode_start);
  p->sz = PGSIZE; // 进程大小为一页

  // 准备进入用户空间
  // 设置trapframe的epc为0, 这是用户程序的入口点
  p->trapframe->epc = 0;
  // 设置用户栈顶, 用户栈位于虚拟地址空间的顶部
  p->trapframe->sp = PGSIZE;

  p->state = RUNNABLE;

  printf("user_init: 第一个进程已创建, 等待调度!\n");
}
