# 测试fork和exit的用户程序
    .equ SYS_write, 16
    .equ SYS_exit, 2
    .equ SYS_fork, 1
    .section .text
    .globl _start
_start:
    li a7, SYS_fork
    ecall
    mv t0, a0
    beqz t0, child
parent:
    li a0, 1
    la a1, pmsg
    la t0, pmsg_end
    la t1, pmsg
    sub a2, t0, t1
    li a7, SYS_write
    ecall
    li a0, 0
    li a7, SYS_exit
    ecall
child:
    li a0, 1
    la a1, cmsg
    la t0, cmsg_end
    la t1, cmsg
    sub a2, t0, t1
    li a7, SYS_write
    ecall
    li a0, 0
    li a7, SYS_exit
    ecall
1:
    j 1b
    .section .rodata
pmsg:
    .ascii "Parent: forked child\n"
pmsg_end:
cmsg:
    .ascii "Child: after fork\n"
cmsg_end:
