#include "types.h"

void uart_puts(const char *s);
void uart_putc(int c);
void panic(const char *s);
int uart_getc(void);
void uart_init(void);
int printf(char *fmt, ...);
void clear_screen(void);

// kalloc.c
void pmm_init();
void* alloc_page(void);
void free_page(void*);
void* alloc_pages(int order);   // 新增: 分配 2^order 个连续页, 返回物理基地址或0
// small object allocator
void* kmalloc(uint64 size);
void kfree(void* p);
void bd_print(); // 打印伙伴系统状态, 仅调试用

// vm.c
void kvm_init();
void kvm_init_hart();

// string.c
void* memset(void*, int, uint);

// trap.c - 中断处理相关函数
void trap_init(void);
void trap_init_hart(void);
int register_interrupt(int irq, void (*handler)(void), const char *name);
void unregister_interrupt(int irq, void (*handler)(void));
void enable_interrupt(int irq);
void disable_interrupt(int irq);
void kerneltrap(void);
int devintr(void);
void handle_exception(uint64 cause, uint64 epc, uint64 tval);

// timer.c - 时钟中断相关函数
void timer_init(void);
void timer_interrupt(void);
uint64 get_time(void);
void set_next_timer(uint64 interval);


void run_all_tests(void);

// 全局变量声明
extern volatile uint64 ticks;
