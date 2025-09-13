#ifndef __PROC_H
#define __PROC_H

#include "types.h"

// Per-CPU state.
struct cpu {
  // To be filled in later labs
};

// Saved registers for kernel context switches.
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

// Per-process state
struct proc {
  // To be filled in later labs
};

// 进程中断时保存的寄存器现场
// 当一个进程在用户态或内核态发生中断/异常时，
// 硬件或软件会把这些寄存器保到栈上。
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表的satp
  /*   8 */ uint64 kernel_sp;     // 内核栈的栈顶指针
  /*  16 */ uint64 kernel_trap;   // kerneltrap()函数的地址
  /*  24 */ uint64 epc;           // 发生中断/异常时的指令地址
  /*  32 */ uint64 kernel_hartid; // 当前CPU的ID
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

#endif // __PROC_H
