    .section .text.start,"ax"
    .globl _start
_start:
    # 设置全局指针，确保小数据段可通过 gp 相对寻址访问
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    addi sp, sp, -16
    sd ra, 0(sp)
    call main
    mv a1, a0
    ld ra, 0(sp)
    addi sp, sp, 16
    mv a0, a1
    call exit
1:
    j 1b
