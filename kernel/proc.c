// 进程管理 (proc.c)

#include "proc.h"
#include "global_func.h"
#include "memlayout.h"

#define NPROC 64 // 最大进程数

struct proc proc[NPROC];
struct proc *initproc;

int nextpid = 1;

void forkret(void);

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
struct proc*
alloc_proc(void)
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

// 创建第一个用户进程
void
user_init(void)
{
  struct proc *p;

  p = alloc_proc();
  if(p == 0)
    panic("user_init: alloc_proc failed");
  
  initproc = p;
  printf("user_init: first process allocated, pid=%d\n", p->pid);

  // 接下来的步骤: 
  // 1. 创建用户页表
  // 2. 加载initcode.S到内存
  // 3. 设置trapframe
  // 4. 将进程状态设置为RUNNABLE
}
