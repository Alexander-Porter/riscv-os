#include "../include/trap.h"
#include "../include/riscv.h"

#include "../global_func.h"
#include "../types.h"


#define TIMER_INTERVAL 100000


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

    
    // 设置下次时钟中断
    set_next_timer(TIMER_INTERVAL);
    
    // 每500次中断打印一次信息，减少输出频率
    if (ticks % 500 == 0) {
        printf("Timer interrupt: ticks=%lu\n", ticks);
    }
}
