// 内核日志系统
// 提供多级别日志记录功能

#ifndef KLOG_H
#define KLOG_H

// 日志级别定义
#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARN    2
#define LOG_ERROR   3
#define LOG_PANIC   4

// 日志缓冲区大小 (4KB)
#define KLOG_BUF_SIZE 4096

// 日志条目结构
struct klog_entry {
    unsigned long timestamp;  // 时间戳（ticks）
    int level;               // 日志级别
    char msg[128];          // 日志消息
};

// 初始化日志系统
void klog_init(void);

// 记录日志
void klog(int level, const char *fmt, ...);

// 获取日志（用户态接口）
int klog_read(char *buf, int n);

// 清空日志缓冲区
void klog_clear(void);

#endif // KLOG_H
