#include "../include/trap.h"
#include "../include/riscv.h"
#include "../global_func.h"
#include "../types.h"

// 测试用的全局变量
static volatile int test_interrupt_count = 0;
static volatile int nested_test_count = 0;
static volatile int shared_test_count_1 = 0;
static volatile int shared_test_count_2 = 0;



/**
 * 测试间隔函数，等待一段时间让系统稳定
 */
void test_delay(void)
{
    uint64 start_ticks = ticks;
    // 等待约2秒（200个tick）
    while (ticks < start_ticks + 200) {
        for (volatile int i = 0; i < 100000; i++);
    }
}

/**
 * 测试用的中断处理函数
 */
void test_handler_1(void)
{
    test_interrupt_count++;
}

void test_handler_2(void)
{
    test_interrupt_count += 2;
}


void shared_handler_1(void)
{
    shared_test_count_1++;
    // 减少输出频率
    if (shared_test_count_1 <= 3) {
        printf("Shared handler 1 called: count=%d\n", shared_test_count_1);
    }
}

void shared_handler_2(void)
{
    shared_test_count_2++;
    // 减少输出频率
    if (shared_test_count_2 <= 3) {
        printf("Shared handler 2 called: count=%d\n", shared_test_count_2);
    }
}


/**
 * 普通的外部中断处理函数
 */
void normal_external_handler(void)
{
    static int call_count = 0;
    call_count++;
    printf("Normal external interrupt handler called, count=%d\n", call_count);
}

/**
 * 测试基本时钟中断功能
 */
void test_timer_interrupt(void)
{
    printf("\n=== Testing Timer Interrupt ===\n");
    
    uint64 start_time = get_time();
    uint64 start_ticks = ticks;
    
    printf("Start time: %lu, start ticks: %lu\n", start_time, start_ticks);
    
    // 等待几次时钟中断
    while (ticks < start_ticks + 5) {
        // 简单延时循环
        for (volatile int i = 0; i < 100000; i++);
    }
    
    uint64 end_time = get_time();
    uint64 end_ticks = ticks;
    
    printf("End time: %lu, end ticks: %lu\n", end_time, end_ticks);
    printf("Timer test completed: %lu ticks in %lu cycles\n", 
           end_ticks - start_ticks, end_time - start_time);
    
    if (end_ticks > start_ticks) {
        printf("Timer interrupt test: PASSED\n");
    } else {
        printf("Timer interrupt test: FAILED\n");
    }
}

/**
 * 测试中断注册和注销
 */
void test_interrupt_registration(void)
{
    printf("\n=== Testing Interrupt Registration ===\n");
    
    // 注册测试中断处理函数
    int ret1 = register_interrupt(IRQ_TIMER, test_handler_1, "test_handler_1");
    int ret2 = register_interrupt(IRQ_TIMER, test_handler_2, "test_handler_2");
    
    if (ret1 == 0 && ret2 == 0) {
        printf("Interrupt registration: PASSED\n");
    } else {
        printf("Interrupt registration: FAILED\n");
        return;
    }
    
    // 等待一些中断
    int old_count = test_interrupt_count;
    uint64 start_ticks = ticks;
    
    while (ticks < start_ticks + 3) {
        for (volatile int i = 0; i < 50000; i++);
    }
    
    printf("Test interrupt count increased by: %d\n", test_interrupt_count - old_count);
    
    // 注销中断处理函数
    unregister_interrupt(IRQ_TIMER, test_handler_1);
    unregister_interrupt(IRQ_TIMER, test_handler_2);
    
    printf("Interrupt registration test completed\n");
}

/**
 * 测试共享中断
 */
void test_shared_interrupt(void)
{
    printf("\n=== Testing Shared Interrupt ===\n");
    
    // 在同一个中断号上注册多个处理函数（共享中断）
    register_interrupt(IRQ_TIMER, shared_handler_1, "shared_1");
    register_interrupt(IRQ_TIMER, shared_handler_2, "shared_2");
    

    uint64 start_ticks = ticks;
    
    // 等待几次中断
    while (ticks < start_ticks + 3) {
        for (volatile int i = 0; i < 50000; i++);
    }
    

    
    // 清理
    unregister_interrupt(IRQ_TIMER, shared_handler_1);
    unregister_interrupt(IRQ_TIMER, shared_handler_2);
}

/**
 * 测试页故障异常处理
 */
void test_page_fault_handling(void)
{
    printf("\n=== Testing Page Fault Exception Handling ===\n");
    
    printf("Testing load page fault by accessing unmapped memory...\n");
    
    // 选择一个真正未映射的地址
    // 我们在vm.c中故意只映射到0x85000000，所以0x86000000是未映射的
    volatile uint64 *test_addr = (uint64*)0x86000000;  // 故意留出的未映射区域
    
    printf("Attempting to read from unmapped address 0x%lx\n", (uint64)test_addr);
    
    // 这会触发页故障，我们的处理函数会分配并映射页面
    uint64 value = *test_addr;
    
    printf("Successfully read value 0x%lx from address 0x%lx\n", value, (uint64)test_addr);

    *test_addr = 0xDEADBEEF;
    
    // 再次读取验证
    uint64 written_value = *test_addr;
    printf("Written 0xDEADBEEF, read back 0x%lx\n", written_value);
    
    if (written_value == 0xDEADBEEF) {
        printf("Page fault handling test: PASSED\n");
    } else {
        printf("Page fault handling test: FAILED\n");
    }
}




// 中断嵌套测试的全局变量
static volatile int test1_called = 0;  // 高优先级中断A是否被调用
static volatile int test2_called = 0;  // 低优先级中断B是否被调用（不应该被调用）
static volatile int test3_called = 0;  // 低优先级中断C是否被调用
static volatile int test3_calling = 0;  // 低优先级中断C是否被调用
static volatile int test4_called = 0;  // 高优先级中断D是否被调用（应该被调用）

/**
 * 测试函数A：高优先级中断（IRQ_EXTERNAL，优先级HIGH）
 * 在处理过程中触发低优先级中断，验证低优先级不能嵌套高优先级
 */
void test_high_priority_handler_a(void)
{
    test1_called = 1;
    printf("Handler A (HIGH priority) started\n");
    
    // 在高优先级中断处理过程中，手动触发低优先级软件中断
    // 这个低优先级中断不应该能够嵌套当前的高优先级中断
    w_sip(r_sip() | (1L << 1));  // 触发软件中断（低优先级）
    
    // 延时一段时间，给低优先级中断机会尝试嵌套
    for (volatile int i = 0; i < 1000000; i++);
    
    printf("Handler A (HIGH priority) finished\n");
}

/**
 * 测试函数B：低优先级中断（IRQ_SOFTWARE，优先级LOW）
 * 如果这个函数被调用，说明低优先级中断错误地嵌套了高优先级中断
 */
void test_low_priority_handler_b(void)
{
    test2_called = 1;  // 如果被调用，测试失败
    printf("Handler B (LOW priority) called - THIS SHOULD NOT HAPPEN!\n");
}

/**
 * 测试函数C：低优先级中断（IRQ_SOFTWARE，优先级LOW）
 * 在处理过程中触发高优先级中断，验证高优先级可以嵌套低优先级
 */
void test_low_priority_handler_c(void)
{
    test3_called = 1;
    test3_calling=1;
    printf("Handler C (LOW priority) started\n");
    
    
    printf("Handler C: About to be interrupted by higher priority\n");
    
    // 延时，在此期间高优先级中断应该能够嵌套进来
    for (volatile int i = 0; i < 500000; i++) {
        // 在延时过程中，如果有高优先级中断，应该能够嵌套进来
        if (i == 250000) {
            // 在延时中间触发时钟中断（中等优先级，比软件中断高）
            // 通过设置stimecmp为当前时间来立即触发时钟中断
            uint64 current_time = r_time();
            w_stimecmp(current_time + 1);  // 设置为立即触发
        }
    }
    test3_calling=0;
    printf("Handler C (LOW priority) finished\n");
}

/**
 * 测试函数D：高优先级中断处理函数（通过时钟中断触发）
 * 如果这个函数被调用，说明高优先级中断成功嵌套了低优先级中断
 */
void test_high_priority_handler_d(void)
{
    test4_called = 1;  // 如果被调用，测试成功
    if (test3_calling){
    printf("Handler D (NORMAL priority) called - nested interrupt SUCCESS!\n");
    }
    // 恢复正常的时钟中断间隔
    uint64 current_time = r_time();
    w_stimecmp(current_time + 10000000);  // 恢复正常间隔
}

/**
 * 测试中断嵌套和优先级
 * 这个测试验证：
 * 1. 高优先级中断不会被低优先级中断嵌套
 * 2. 低优先级中断可以被高优先级中断嵌套
 */
void test_irq_priority_and_nesting(void)
{
    printf("\n=== Testing Interrupt Priority and Nesting ===\n");
    
    // 重置测试标志
    test1_called = 0;
    test2_called = 0;
    test3_called = 0;
    test4_called = 0;
    
    printf("Phase 1: Testing high priority cannot be nested by low priority\n");
    
    // 注册测试处理函数
    register_interrupt(IRQ_EXTERNAL, test_high_priority_handler_a, "test_high_a");
    register_interrupt(IRQ_SOFTWARE, test_low_priority_handler_b, "test_low_b");
    

    
    // 外部中断的发生
    handle_interrupt_chain(IRQ_EXTERNAL);
    
    // 检查结果
    if (test1_called && !test2_called) {
        printf("Test 1 PASSED: High priority was not interrupted by low priority\n");
    } else {
        printf("Test 1 FAILED: test1_called=%d, test2_called=%d\n", test1_called, test2_called);
    }
    
    // 清理第一阶段的处理函数
    unregister_interrupt(IRQ_EXTERNAL, test_high_priority_handler_a);
    unregister_interrupt(IRQ_SOFTWARE, test_low_priority_handler_b);
    
    printf("\nPhase 2: Testing low priority can be nested by high priority\n");
    
    // 注册第二阶段的测试处理函数
    register_interrupt(IRQ_SOFTWARE, test_low_priority_handler_c, "test_low_c");
    register_interrupt(IRQ_TIMER, test_high_priority_handler_d, "test_high_d");
    
    // 触发低优先级软件中断
    printf("Triggering low priority interrupt (software)...\n");
    
    // 模拟软件中断的发生
    handle_interrupt_chain(IRQ_SOFTWARE);
    
    // 给一点时间让可能的嵌套中断完成
    for (volatile int i = 0; i < 100000; i++);
    
    // 检查结果
    if (test3_called && test4_called) {
        printf("Test 2 PASSED: Low priority was correctly interrupted by high priority\n");
    } else {
        printf("Test 2 FAILED: test3_called=%d, test4_called=%d\n", test3_called, test4_called);
    }
    
    // 清理第二阶段的处理函数
    unregister_interrupt(IRQ_SOFTWARE, test_low_priority_handler_c);
    unregister_interrupt(IRQ_TIMER, test_high_priority_handler_d);
    

}

/**
 * 运行核心测试（符合实验四要求）
 */
void run_all_tests(void)
{
    printf("\n========== Lab 4 Core Tests ==========\n");
    
    // 任务3&5: 时钟中断测试
    test_timer_interrupt();
    test_delay();
    

    
    // 任务3: 共享中断测试
    test_shared_interrupt();
    test_delay();
    
    // 任务3: 中断嵌套测试
    test_irq_priority_and_nesting();
    test_delay();
    
    // 任务6: 页故障异常处理测试
    test_page_fault_handling();
    test_delay();
    

    
    printf("\n========== Lab 4 Tests Completed ==========\n");
}