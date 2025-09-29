#include "include/riscv.h"
#include "include/trap.h"
#include "global_func.h"
#include "paging.h"

void main();
void timer_init_machine(void);

// entry.S needs one stack per CPU.
__attribute__((aligned(16))) char stack0[4096];

// entry.S jumps here in machine mode on stack0.
void start()
{
    // 设置M模式的前一个特权级为S模式，用于mret
    uint64 x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // 设置M模式异常程序计数器为main，用于mret
    w_mepc((uint64)main);

    // 暂时禁用分页
    w_satp(0);

    // 将所有中断和异常委托给监督模式
    w_medeleg(0xffff);
    w_mideleg(0xffff);
    w_sie(r_sie() | SIE_SEIE | SIE_STIE);

    // 配置物理内存保护，给监督模式访问所有物理内存的权限
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    // 初始化时钟中断
    timer_init_machine();

    // 切换到监督模式并跳转到main()
    asm volatile("mret");
}

// 在机器模式下初始化时钟中断
void timer_init_machine()
{
    // 启用监督模式时钟中断
    w_mie(r_mie() | MIE_STIE);
    
    // 启用sstc扩展 (stimecmp)
    w_menvcfg(r_menvcfg() | (1L << 63)); 
    
    // 允许监督模式使用stimecmp和time
    w_mcounteren(r_mcounteren() | 2);
    
    // 请求第一次时钟中断
    w_stimecmp(r_time() + 1000000);
}
