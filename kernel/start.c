#include "global_func.h"

// entry.S needs one stack per CPU.
__attribute__((aligned(16))) char stack0[4096];
int bss_test; // 用于测试.bss段是否被清零
float bss_test_float; // 用于测试.bss段是否被清零
// entry.S jumps here in S-mode on stack0.
void start()
{
    // 设置S模式的中断处理
    trapinithart();
    
    // 调用内核主函数
    main();
}
