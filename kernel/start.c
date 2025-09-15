#include "global_func.h"
void main();

int bss_test; // 用于测试 .bss 段是否被正确清零
float bss_test_float; // 测试 .bss 段的浮点数变量

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096];

// entry.S jumps here in machine mode on stack0.
void
start()
{
uart_putc('T'); // 输出字符S表示启动
uart_puts("Hello, world!\n");
if (bss_test != 0 || bss_test_float != 0.0f) {
    uart_puts("Error: .bss segment not cleared!\n");
} else {
    uart_puts(".bss segment cleared, at least we have zero for bss_test and bss_test_float.\n");
}


}
