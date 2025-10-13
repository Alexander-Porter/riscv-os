    .equ SYS_fork, 1
    .equ SYS_exit, 2
    .equ SYS_wait, 3
    .equ SYS_getpid, 11
    .equ SYS_write, 16
    .equ SYS_yield, 22
    .equ SYS_setpriority, 23
    .equ SYS_getpriority, 24
    .equ SYS_getrunticks, 25

    .section .text
    .globl _start

_start:
    # 打印测试标题
    li a0, 1
    la a1, msg_start
    la t0, msg_start_end
    la t1, msg_start
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    # 创建高优先级子进程
    li a7, SYS_fork
    ecall
    beqz a0, child_high
    mv s0, a0             # 记录高优先级子进程PID

    # 创建低优先级子进程
    li a7, SYS_fork
    ecall
    beqz a0, child_low

parent_wait:
    # 等待两个子进程结束
    li s1, 0
wait_loop:
    li a0, 0
    li a7, SYS_wait
    ecall
    addi s1, s1, 1
    li t0, 2
    blt s1, t0, wait_loop

    # 打印完成信息
    li a0, 1
    la a1, msg_done
    la t0, msg_done_end
    la t1, msg_done
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    li a0, 0
    li a7, SYS_exit
    ecall

child_high:
    # 设置最高优先级
    li a0, 0
    li a7, SYS_setpriority
    ecall

    # 确认优先级设置
    li a7, SYS_getpriority
    ecall
    mv a0, a0
    jal ra, print_priority_high

    # 执行计算密集型循环
        li t0, 20000000
1:
    addi t0, t0, -1
    bnez t0, 1b

    # 读取运行tick并打印
    li a7, SYS_getrunticks
    ecall
    jal ra, print_ticks_high

    li a0, 0
    li a7, SYS_exit
    ecall

child_low:
    # 设置低优先级
    li a0, 2
    li a7, SYS_setpriority
    ecall

    # 确认优先级设置
    li a7, SYS_getpriority
    ecall
    mv a0, a0
    jal ra, print_priority_low

    # 执行计算密集型循环
        li t0, 40000000
2:
    addi t0, t0, -1
    bnez t0, 2b

    # 读取运行tick并打印
    li a7, SYS_getrunticks
    ecall
    jal ra, print_ticks_low

    li a0, 0
    li a7, SYS_exit
    ecall

# 打印高优先级子进程的优先级
print_priority_high:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0

    li a0, 1
    la a1, msg_high_prio
    la t0, msg_high_prio_end
    la t1, msg_high_prio
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    mv a0, s0
    jal ra, print_decimal

    li a0, 1
    la a1, msg_newline
    la t0, msg_newline_end
    la t1, msg_newline
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# 打印低优先级子进程的优先级
print_priority_low:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0

    li a0, 1
    la a1, msg_low_prio
    la t0, msg_low_prio_end
    la t1, msg_low_prio
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    mv a0, s0
    jal ra, print_decimal

    li a0, 1
    la a1, msg_newline
    la t0, msg_newline_end
    la t1, msg_newline
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# 打印高优先级子进程运行tick
print_ticks_high:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0

    li a0, 1
    la a1, msg_high_ticks
    la t0, msg_high_ticks_end
    la t1, msg_high_ticks
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    mv a0, s0
    jal ra, print_decimal

    li a0, 1
    la a1, msg_newline
    la t0, msg_newline_end
    la t1, msg_newline
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# 打印低优先级子进程运行tick
print_ticks_low:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0

    li a0, 1
    la a1, msg_low_ticks
    la t0, msg_low_ticks_end
    la t1, msg_low_ticks
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    mv a0, s0
    jal ra, print_decimal

    li a0, 1
    la a1, msg_newline
    la t0, msg_newline_end
    la t1, msg_newline
    sub a2, t0, t1
    li a7, SYS_write
    ecall

    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# 打印无符号整数（十进制）
print_decimal:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    sd s6, 0(sp)

    mv s0, a0
    la s1, num_buf_end

    beqz s0, print_decimal_zero

print_decimal_loop:
    li t0, 10
    remu t1, s0, t0
    addi t1, t1, 48
    addi s1, s1, -1
    sb t1, 0(s1)
    divu s0, s0, t0
    bnez s0, print_decimal_loop
    j print_decimal_output

print_decimal_zero:
    addi s1, s1, -1
    li t1, 48
    sb t1, 0(s1)

print_decimal_output:
    li a0, 1
    mv a1, s1
    la t0, num_buf_end
    sub a2, t0, s1
    li a7, SYS_write
    ecall

    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    ld s6, 0(sp)
    addi sp, sp, 64
    ret

    .section .rodata
msg_start:
    .ascii "testsched: parent start\n"
msg_start_end:
msg_done:
    .ascii "testsched: done\n"
msg_done_end:
msg_high_prio:
    .ascii "high priority set to "
msg_high_prio_end:
msg_low_prio:
    .ascii "low priority set to "
msg_low_prio_end:
msg_high_ticks:
    .ascii "high priority ticks="
msg_high_ticks_end:
msg_low_ticks:
    .ascii "low priority ticks="
msg_low_ticks_end:
msg_newline:
    .ascii "\n"
msg_newline_end:

    .section .bss
    .align 3
num_buf:
    .space 32
num_buf_end:
