#include "magic_values.h"
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

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

void
uart_init(void)
{
  // disable interrupts.
  WriteReg(IER, 0x00);

  // special mode to set baud rate.
  WriteReg(LCR, LCR_BAUD_LATCH);

  // LSB for baud rate of 38.4K.
  WriteReg(0, 0x03);

  // MSB for baud rate of 38.4K.
  WriteReg(1, 0x00);

  // leave set-baud mode,
  // and set word length to 8 bits, no parity.
  WriteReg(LCR, LCR_EIGHT_BITS);

  // reset and enable FIFOs.
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // enable transmit and receive interrupts.
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

  // 我们现在不使用中断驱动的 UART 输出
  //initlock(&tx_lock, "uart");
}