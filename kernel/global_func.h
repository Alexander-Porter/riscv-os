void uart_puts(const char *s);
void uart_putc(int c);
void panic(const char *s);
int uart_getc(void);
void uart_init(void);