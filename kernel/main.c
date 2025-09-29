#include "global_func.h"
#include "memlayout.h"
#include "paging.h"      // 引入 pagetable_t 与 dump_pagetable 原型
#include "include/trap.h"
#include "include/riscv.h"
#include "types.h"

// 声明测试函数
void run_all_tests(void);
void default_timer_handler(void);
void high_priority_timer_handler(void);
void low_priority_timer_handler(void);

// 内核主函数
void main()
{
    printf("Hello OS - Lab 4: Interrupt Handling\n");

    pmm_init();         // 初始化物理内存管理器
    
    kvm_init();         // 创建内核页表
    kvm_init_hart();    // 启用分页

    // 初始化中断系统
    printf("Initializing interrupt system...\n");
    trap_init();        // 初始化中断系统
    trap_init_hart();   // 初始化当前hart的中断处理
    timer_init();       // 初始化时钟中断（会自动注册system_timer_handler）

    // 注册一些额外的时钟中断处理函数来演示共享中断
    // 注意：system_timer_handler已经在timer_init()中注册
    register_interrupt(IRQ_TIMER, default_timer_handler, "default_timer");
    register_interrupt(IRQ_TIMER, low_priority_timer_handler, "low_priority_timer");

    // 启用中断
    enable_interrupt(IRQ_TIMER);
    intr_on();

    printf("Interrupt system initialized successfully\n");
    printf("Starting interrupt tests...\n");

    // 运行测试用例
    run_all_tests();

    printf("All tests completed\n");
    
    // 保持系统运行，让时钟中断继续工作
    printf("System is running... Press Ctrl+C to exit\n");
    while (1) {
        // 简单的空闲循环
        for (volatile int i = 0; i < 10000000; i++);
        
        // 每隔一段时间打印系统状态
        static int status_count = 0;
        status_count++;
        if (status_count % 500000 == 0) {
            printf("System status: ticks=%lu, time=%lu\n", ticks, get_time());
        }
    }
}
