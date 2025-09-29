#ifndef __TRAP_H
#define __TRAP_H

#include "../types.h"

// 中断处理函数类型
typedef void (*interrupt_handler_t)(void);

// 系统调用处理函数类型
typedef void (*syscall_handler_t)(struct trapframe *tf);

// 中断描述符结构体 - 支持共享中断（类似Linux）
struct interrupt_desc {
    interrupt_handler_t handler;    // 中断处理函数
    struct interrupt_desc *next;    // 下一个中断描述符 (用于共享中断链表)
    struct interrupt_desc *prev;    // 上一个中断描述符 (双向链表)
    int irq;                       // 中断号
    char name[32];                 // 中断名称
};

// 中断向量表大小
#define MAX_IRQ_NUM 64

// 中断类型定义 - 对应RISC-V scause的中断位
#define IRQ_SOFTWARE 1   // 软件中断 (scause = 0x8000000000000001)
#define IRQ_TIMER    5   // 时钟中断 (scause = 0x8000000000000005)
#define IRQ_EXTERNAL 9   // 外部中断 (scause = 0x8000000000000009)

// scause到IRQ的转换宏
#define SCAUSE_TO_IRQ(scause) ((scause) & 0xf)

// 中断优先级定义
#define IRQ_PRIORITY_HIGH    0
#define IRQ_PRIORITY_NORMAL  1
#define IRQ_PRIORITY_LOW     2

// 中断控制接口
void trap_init(void);
void trap_init_hart(void);
int register_interrupt(int irq, interrupt_handler_t handler, const char *name);
void unregister_interrupt(int irq, interrupt_handler_t handler);
void enable_interrupt(int irq);
void disable_interrupt(int irq);

// 中断处理函数
void kerneltrap(void);
int devintr(void);

// 时钟中断相关
void timer_init(void);
void system_timer_handler(void);
uint64 get_time(void);
void set_next_timer(uint64 interval);

// 异常处理
void handle_exception(uint64 cause, uint64 epc, uint64 tval);

// 测试函数
void test_timer_interrupt(void);
void test_page_fault_handling(void);
void test_shared_interrupt(void);
void test_irq_priority_and_nesting(void);
void test_linux_style_interrupt(void);
void test_exception_handling(void);
void run_all_tests(void);

// 全局变量声明
extern volatile uint64 ticks;
extern struct interrupt_desc *interrupt_table[MAX_IRQ_NUM];

#endif