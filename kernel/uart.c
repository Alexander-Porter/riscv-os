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