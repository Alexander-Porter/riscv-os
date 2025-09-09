#include "magic_values.h"

void uart_putc(int c) {
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);
}

int uart_getc(void) {
  if(ReadReg(LSR) & LSR_RX_READY){
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

void uart_puts(const char *s) {
  while(*s) {
    if(*s == '\n')
      uart_putc('\r');
    uart_putc(*s++);
  }
}

void panic(const char *s) {
  uart_puts("panic: ");
  uart_puts(s);
  uart_puts("\n");
  while(1);
}

void uart_getstr(char *buf, int max) {
  int i = 0;
  while(i + 1 < max) {
    int c = uart_getc();
    if(c == -1)
      continue;
    if(c == '\r')
      break;
    uart_putc(c);
    buf[i++] = c;
  }
  buf[i] = 0;
  uart_putc('\n');
}