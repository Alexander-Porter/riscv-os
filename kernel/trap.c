// 中断与异常处理 (trap.c)

#include "types.h"
#include "paging.h"
#include "global_func.h"
#include "sbi.h"

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
  uint64 scause = r_scause();
  uint64 sepc = r_sepc();

  // 判断是中断还是异常
  if (scause & (1UL << 63)) { // 最高位为1, 表示是中断
    // 进一步判断中断类型, 这里我们只关心S模式时钟中断
    if ((scause & 0x7FFFFFFFFFFFFFFF) == 5) {
      // 是S模式的时钟中断
      // 重新设置时钟中断 (10,000,000 apx. 1s on QEMU)
      sbi_set_timer(r_time() + 10000000);

      // 临时打印信息, 确认中断已收到
      printf("timer interrupt!\n");

    } else {
      printf("unhandled interrupt: scause %p, sepc %p\n", scause, sepc);
      panic("kerneltrap");
    }
  } else { // 是异常
    printf("exception: scause %p, sepc %p\n", scause, sepc);
    panic("kerneltrap");
  }
}

