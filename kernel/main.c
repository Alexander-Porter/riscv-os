#include "global_func.h"
#include "memlayout.h"
#include "paging.h"      // 引入 pagetable_t 与 dump_pagetable 原型
#include "include/trap.h"
#include "include/riscv.h"
#include "proc.h"
#include "types.h"
#include "semaphore.h"

// 内核主函数
void main()
{
    printf("Hello OS - Lab 5: Process Management\n");

    pmm_init();         // 初始化物理内存管理器
    kvm_init();         // 创建内核页表并映射内核栈
    kvm_init_hart();    // 启用分页

    procinit();         // 初始化进程表
    semaphore_system_init(); // 初始化内核同步原语

    trap_init();        // 初始化中断/异常子系统
    trap_init_hart();   // 安装监督态陷阱向量
    timer_init();       // 初始化时钟中断

    enable_interrupt(IRQ_TIMER);

    userinit();         // 创建第一个用户进程

    scheduler();        // 进入调度循环，不会返回
}
