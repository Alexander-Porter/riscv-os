#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/sbi.h"
#include "../global_func.h"
#include "../types.h"

// 时钟中断间隔 (约100Hz)
#define TIMER_INTERVAL 100000

// 时钟中断统计
static volatile uint64 timer_count = 0;

/**
 * 初始化时钟中断
 */
void timer_init(void)
{
    // 注册系统时钟处理函数（确保系统时钟正常工作）
    register_interrupt(IRQ_TIMER, system_timer_handler, "system_timer");
    
    // 启用监督模式时钟中断
    w_sie(r_sie() | SIE_STIE);
    
    // 设置第一次时钟中断
    set_next_timer(TIMER_INTERVAL);
    
    printf("Timer initialized with interval %d\n", TIMER_INTERVAL);
}

/**
 * 系统时钟中断处理函数 - 负责基本的时钟管理
 */
void system_timer_handler(void)
{
    // 更新系统时钟
    ticks++;
    timer_count++;
    
    // 设置下次时钟中断
    set_next_timer(TIMER_INTERVAL);
    
    // 每500次中断打印一次信息，减少输出频率
    if (timer_count % 500 == 0) {
        printf("Timer interrupt: ticks=%lu, count=%lu\n", 
               ticks, timer_count);
    }
}

/**
 * 默认时钟中断处理函数
 */
void default_timer_handler(void)
{
    // 这里可以添加调度逻辑
    // 目前只是简单的计数
}

/**
 * 高优先级时钟处理函数
 */
void high_priority_timer_handler(void)
{
    // 高优先级任务处理
    static int high_count = 0;
    high_count++;
    
    if (high_count % 250 == 0) {
        printf("High priority timer handler: count=%d\n", high_count);
    }
}

/**
 * 低优先级时钟处理函数
 */
void low_priority_timer_handler(void)
{
    // 低优先级任务处理
    static int low_count = 0;
    low_count++;
    
    if (low_count % 500 == 0) {
        printf("Low priority timer handler: count=%d\n", low_count);
    }
}