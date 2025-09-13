#include "types.h"

// main.c
void main();

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

// trap.c
void trapinithart(void);
void kerneltrap();

// string.c
void* memset(void*, int, uint);
