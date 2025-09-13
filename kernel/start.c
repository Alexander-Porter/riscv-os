#include "global_func.h"

// entry.S needs one stack per CPU.
__attribute__((aligned(16))) char stack0[4096];

// entry.S jumps here in S-mode on stack0.
void start()
{
    // 设置S模式的中断处理
    trapinithart();
    
    // 调用内核主函数
    main();
}
