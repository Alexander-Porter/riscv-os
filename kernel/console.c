//
// 控制台输入输出，使用 UART。
// 读取按行进行。
// 实现特殊输入字符：
//   换行 -- 行结束
//   Control-H -- 退格
//   Control-U -- 删除整行
//   Control-D -- 文件结束
//   Control-P -- 打印进程列表
//

#include "types.h"
#include "global_func.h"

// 输出单个字符到 UART
void console_putc(char c) {
    uart_putc(c);
}

// 输出字符串到 UART
void console_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            console_putc('\r');
        }
        console_putc(*s++);
    }
}
