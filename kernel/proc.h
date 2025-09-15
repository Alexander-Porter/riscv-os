#ifndef __PROC_H
#define __PROC_H

#include "types.h"
#include "paging.h"

// 内核上下文切换时保存的寄存器
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
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

// 每个CPU核心的状态
struct cpu {
  struct proc *proc;          // 当前在CPU上运行的进程, 如果没有则为null
  struct context context;     // 调度器的上下文, swtch切换到这里来进入调度器
  int noff;                   // 关中断的嵌套深度
  int intena;                 // 在关中断之前, 中断是否是开启的
};

extern struct cpu cpus[1]; // 目前只支持单核

// 用户态陷入内核时，保存的用户寄存器和上下文信息
// 这个结构体需要和kernelvec.S中的寄存器保存/恢复顺序严格对应
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表
  /*   8 */ uint64 kernel_sp;     // 进程内核栈顶
  /*  16 */ uint64 kernel_trap;   // usertrap()函数的地址
  /*  24 */ uint64 epc;           // 保存的用户PC
  /*  32 */ uint64 kernel_hartid; // a new field could be added here
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

// 进程状态
enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 进程控制块 (PCB)
struct proc {
  enum procstate state;        // 进程状态
  int pid;                     // 进程ID
  uint64 kstack;               // 进程的内核栈地址
  uint64 sz;                   // 进程内存大小 (bytes)
  pagetable_t pagetable;       // 用户页表
  struct trapframe *trapframe; // 指向trapframe页
  struct context context;      // 上下文切换时保存的寄存器
  char name[16];               // 进程名 (用于调试)
};

#endif // __PROC_H
