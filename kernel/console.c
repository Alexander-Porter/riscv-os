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
#include "proc.h"
#include "global_func.h"

void console_init(void) {
    uart_init();
}

// 输出单个字符到 UART
void console_putc(int c) {
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

// 将缓冲区写到控制台，返回写入的字节数
int console_write(const char *buf, int n) {
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\n') {
            console_putc('\r');
        }
        console_putc(buf[i]);
    }
    return n;
}

// 从控制台读取数据，简单地逐字节阻塞等待
int console_read(char *buf, int n) {
    int i = 0;
    while (i < n) {
        int c = uart_getc();
        if (c < 0) {
            struct proc *p = myproc();
            if (p && killed(p)) {
                return -1;
            }
            if (p)
                yield();
            continue;
        }
        if (c == '\r')
            c = '\n';
        buf[i++] = (char)c;
        if (c == '\n') {
            break;
        }
    }
    return i;
}
