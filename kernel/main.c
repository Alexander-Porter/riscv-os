#include "global_func.h"
void console_puts(const char *s);
void main(void)
{
    uart_init();
    console_puts("Hello console");
    while (1)
        ;
}
