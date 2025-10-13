# This initcode writes a message using the write syscall and then exits.

    .equ SYS_write, 16
    .equ SYS_exit, 2

    .section .text
    .globl _start
_start:
    li a0, 1                       # stdout file descriptor
    la a1, message                 # buffer pointer
    la t0, message_end
    la t1, message
    sub a2, t0, t1                 # buffer length
    li a7, SYS_write
    ecall

    li a0, 0
    li a7, SYS_exit
    ecall

1:
    j 1b

    .section .rodata
message:
    .ascii "Hello from initcode!\n"
message_end:
