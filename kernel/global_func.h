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

// vm.c
void kvm_init();
void kvm_init_hart();

// string.c
void* memset(void*, int, uint);
