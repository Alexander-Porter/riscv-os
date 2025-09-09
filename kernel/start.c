#include "global_func.h"
void main();


// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096];

// entry.S jumps here in machine mode on stack0.
void
start()
{
uart_putc('T'); // 输出字符S表示启动
uart_puts("Hello, world!\n");
}

