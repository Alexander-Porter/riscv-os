# 测试系统调用的用户程序
    .equ SYS_write, 16
    .equ SYS_exit, 2
    .equ SYS_getpid, 11
    .section .text
    .globl _start
_start:
    li a0, 1
    la a1, msg
    la t0, msg_end
    la t1, msg
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    li a7, SYS_getpid
    ecall
    mv t0, a0
    li a0, 1
    la a1, pidmsg
    la t0, pidmsg_end
    la t1, pidmsg
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    # 退出
    li a0, 0
    li a7, SYS_exit
    ecall
1:
    j 1b

    .section .rodata
msg:
    .ascii "Test syscall: write & getpid\n"
msg_end:
pidmsg:
    .ascii "PID syscall returned.\n"
pidmsg_end:
