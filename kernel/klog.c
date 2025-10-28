// 内核日志系统实现
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "klog.h"
#include "global_func.h"

// 日志缓冲区（环形缓冲区）
static struct {
    struct spinlock lock;
    char buf[KLOG_BUF_SIZE];
    int write_pos;  // 写入位置
    int read_pos;   // 读取位置
    int count;      // 当前字符数
} klog_buf;

static const char *level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "PANIC"
};

// 初始化日志系统
void klog_init(void)
{
    initlock(&klog_buf.lock, "klog");
    klog_buf.write_pos = 0;
    klog_buf.read_pos = 0;
    klog_buf.count = 0;
}

// 向环形缓冲区写入一个字符
static void klog_putc(char c)
{
    klog_buf.buf[klog_buf.write_pos] = c;
    klog_buf.write_pos = (klog_buf.write_pos + 1) % KLOG_BUF_SIZE;
    if (klog_buf.count < KLOG_BUF_SIZE) {
        klog_buf.count++;
    } else {
        // 缓冲区满，移动读指针
        klog_buf.read_pos = (klog_buf.read_pos + 1) % KLOG_BUF_SIZE;
    }
}

// 简化的格式化字符串到缓冲区（支持基本格式）
static void klog_puts(const char *s)
{
    while (*s) {
        klog_putc(*s++);
    }
}

// 记录日志（支持基本格式化）
void klog(int level, const char *fmt, ...)
{
    if (level < LOG_DEBUG || level > LOG_PANIC)
        return;
    
    acquire(&klog_buf.lock);
    
    // 写入级别前缀
    klog_putc('[');
    klog_puts(level_names[level]);
    klog_puts("] ");
    
    // 简化：只支持%s和普通文本，完整实现需要vsnprintf
    const char *p = fmt;
    while (*p) {
        if (*p == '%' && *(p+1) == 's') {
            // 简单的%s支持（实际使用中主要用于消息）
            p += 2;
        } else {
            klog_putc(*p++);
        }
    }
    klog_putc('\n');
    
    release(&klog_buf.lock);
    
    // 同时输出到控制台（用于调试）
    if (level >= LOG_WARN) {
        printf("[%s] %s\n", level_names[level], fmt);
    }
}

// 读取日志缓冲区
int klog_read(char *buf, int n)
{
    if (buf == 0 || n <= 0)
        return -1;
    
    acquire(&klog_buf.lock);
    
    int i = 0;
    while (i < n - 1 && klog_buf.count > 0) {
        buf[i++] = klog_buf.buf[klog_buf.read_pos];
        klog_buf.read_pos = (klog_buf.read_pos + 1) % KLOG_BUF_SIZE;
        klog_buf.count--;
    }
    buf[i] = '\0';
    
    release(&klog_buf.lock);
    return i;
}

// 清空日志缓冲区
void klog_clear(void)
{
    acquire(&klog_buf.lock);
    klog_buf.write_pos = 0;
    klog_buf.read_pos = 0;
    klog_buf.count = 0;
    release(&klog_buf.lock);
}
