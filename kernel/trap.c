// 中断与异常处理 (trap.c)

#include "types.h"
#include "paging.h"
#include "global_func.h"

// kernelvec.S 中断向量表的地址
extern void kernelvec();

// 设置S模式下的中断处理
void
trapinithart(void)
{
  // 将中断处理总入口地址写入stvec寄存器
  w_stvec((uint64)kernelvec);

  // 使能S模式下的时钟中断、外部中断和软件中断
  w_sie(r_sie() | SIE_STIE | SIE_SEIE | SIE_SSIE);

  // 全局使能S模式下的中断
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// 内核态中断/异常的总处理入口
void kerneltrap()
{
  // 这是一个临时的实现, 仅用于测试
  printf("a trap from kernel! scause=%p sepc=%p\n", r_scause(), r_sepc());
  panic("kerneltrap");
}

