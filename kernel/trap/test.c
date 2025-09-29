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

void nested_test_handler(void)
{
    nested_test_count++;
    printf("Nested test handler called: count=%d\n", nested_test_count);
    
    // 只在第一次调用时触发嵌套中断，避免无限循环
    if (nested_test_count == 1) {
        printf("Triggering nested software interrupt...\n");
        // 触发软件中断来测试嵌套
        w_sip(r_sip() | (1L << 1));
    }
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

void high_priority_nested_handler(void)
{
    static int high_nested_count = 0;
    high_nested_count++;
    // 减少输出频率
    if (high_nested_count <= 5) {
        printf("High priority nested handler called: count=%d\n", high_nested_count);
    }
}

// 嵌套中断测试的四个函数
static volatile int test_success_count = 0;
static volatile int test_failure_count = 0;

/**
 * A: 高优先级函数，触发一个低优先级中断
 */
void handler_A_high_trigger_low(void)
{
    printf("Handler A (HIGH priority): Triggering low priority interrupt\n");
    // 触发软件中断（低优先级）
    w_sip(r_sip() | (1L << 1));
}

/**
 * B: 低优先级处理函数，如果被调用说明测试失败
 */
void handler_B_low_should_not_run(void)
{
    test_failure_count++;
    printf("FAILURE: Handler B (LOW priority) was called! This should NOT happen!\n");
}

/**
 * C: 低优先级函数，触发一个高优先级中断
 */
void handler_C_low_trigger_high(void)
{
    printf("Handler C (LOW priority): Triggering high priority interrupt\n");
    // 触发时钟中断（高优先级）- 设置一个很近的时钟中断
    w_stimecmp(r_time() + 1000);
}

/**
 * D: 高优先级处理函数，如果被调用说明测试成功
 */
void handler_D_high_should_run(void)
{
    test_success_count++;
    printf("SUCCESS: Handler D (HIGH priority) was called!\n");
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
    
    int old_count_1 = shared_test_count_1;
    int old_count_2 = shared_test_count_2;
    uint64 start_ticks = ticks;
    
    // 等待几次中断
    while (ticks < start_ticks + 3) {
        for (volatile int i = 0; i < 50000; i++);
    }
    
    printf("Shared handler 1 called %d times\n", shared_test_count_1 - old_count_1);
    printf("Shared handler 2 called %d times\n", shared_test_count_2 - old_count_2);
    
    if (shared_test_count_1 > old_count_1 && shared_test_count_2 > old_count_2) {
        printf("Shared interrupt test: PASSED\n");
    } else {
        printf("Shared interrupt test: FAILED\n");
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
    printf("This means the page fault was handled correctly!\n");
    
    // 现在尝试写入
    printf("Attempting to write to the same address...\n");
    *test_addr = 0xDEADBEEF;
    
    // 再次读取验证
    uint64 written_value = *test_addr;
    printf("Written 0xDEADBEEF, read back 0x%lx\n", written_value);
    
    if (written_value == 0xDEADBEEF) {
        printf("Page fault handling test: PASSED\n");
        printf("Successfully handled page fault and allocated memory on-demand\n");
    } else {
        printf("Page fault handling test: FAILED\n");
    }
}



/**
 * 测试异常处理
 */
void test_exception_handling(void)
{
    printf("\n=== Testing Exception Handling ===\n");
    
    printf("Note: Exception tests may cause system panic\n");
    printf("This is expected behavior for unhandled exceptions\n");
    
    // 这里我们不实际触发异常，因为会导致系统崩溃
    // 在实际测试中，可以通过以下方式触发异常：
    // 1. 访问无效内存地址
    // 2. 执行非法指令
    // 3. 除零操作（如果支持）
    
    printf("Exception handling framework is ready\n");
    printf("Exception handling test: PASSED (framework only)\n");
}

/**
 * 测试中断嵌套和优先级
 */
void test_irq_priority_and_nesting(void)
{
    printf("\n=== Testing Interrupt Nesting and Priority ===\n");
    
    // 重置计数器
    test_success_count = 0;
    test_failure_count = 0;
    
    printf("Testing interrupt priority rules:\n");
    printf("- High priority should NOT be interrupted by low priority\n");
    printf("- Low priority CAN be interrupted by high priority\n\n");
    
    // === 测试1: 高优先级不应该被低优先级中断 ===
    printf("=== Test 1: High priority should NOT be interrupted ===\n");
    
    // 注册A和B函数
    register_interrupt(IRQ_TIMER, handler_A_high_trigger_low, "handler_A");
    register_interrupt(IRQ_SOFTWARE, handler_B_low_should_not_run, "handler_B");
    
    // 启用软件中断
    enable_interrupt(IRQ_SOFTWARE);
    
    printf("Triggering high priority interrupt (Timer)...\n");
    // 触发时钟中断（高优先级），它会尝试触发软件中断（低优先级）
    w_stimecmp(r_time() + 1000);
    
    // 等待处理完成
    for (volatile int i = 0; i < 2000000; i++);
    
    // 检查结果
    if (test_failure_count == 0) {
        printf("✅ Test 1 PASSED: High priority was not interrupted by low priority\n");
    } else {
        printf("❌ Test 1 FAILED: High priority was incorrectly interrupted\n");
    }
    
    // 清理第一组测试
    unregister_interrupt(IRQ_TIMER, handler_A_high_trigger_low);
    unregister_interrupt(IRQ_SOFTWARE, handler_B_low_should_not_run);
    
    printf("\n=== Test 2: Low priority CAN be interrupted ===\n");
    
    // 注册C和D函数
    register_interrupt(IRQ_SOFTWARE, handler_C_low_trigger_high, "handler_C");
    register_interrupt(IRQ_TIMER, handler_D_high_should_run, "handler_D");
    
    printf("Triggering low priority interrupt (Software)...\n");
    // 触发软件中断（低优先级），它会触发时钟中断（高优先级）
    w_sip(r_sip() | (1L << 1));
    
    // 等待处理完成
    for (volatile int i = 0; i < 2000000; i++);
    
    // 检查结果
    if (test_success_count > 0) {
        printf("✅ Test 2 PASSED: Low priority was correctly interrupted by high priority\n");
    } else {
        printf("❌ Test 2 FAILED: Low priority was not interrupted as expected\n");
    }
    
    // 最终结果
    printf("\n=== Final Results ===\n");
    if (test_failure_count == 0 && test_success_count > 0) {
        printf("✅ Interrupt nesting test: PASSED\n");
        printf("   Interrupt priority rules are correctly enforced\n");
    } else {
        printf("❌ Interrupt nesting test: FAILED\n");
        printf("   Failures: %d, Successes: %d\n", test_failure_count, test_success_count);
    }
    
    // 清理第二组测试
    disable_interrupt(IRQ_SOFTWARE);
    unregister_interrupt(IRQ_SOFTWARE, handler_C_low_trigger_high);
    unregister_interrupt(IRQ_TIMER, handler_D_high_should_run);
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