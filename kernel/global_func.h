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
void free_pages(void* pa, int order); // 新增: 释放按 2^order 页大小的块
// small object allocator
void* kmalloc(uint64 size);
void kfree(void* p);

// vm.c
void kvm_init();
void kvm_init_hart();

// string.c
void* memset(void*, int, uint);
