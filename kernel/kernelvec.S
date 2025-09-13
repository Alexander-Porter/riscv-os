.section .text
.globl kernelvec
.align 4
kernelvec:
    // 保存所有通用寄存器到trapframe
    // The order is defined by the trapframe struct in proc.h.
    csrrw sp, sscratch, sp
    sd ra, 40(sp)
    sd gp, 56(sp)
    sd tp, 64(sp)
    sd t0, 72(sp)
    sd t1, 80(sp)
    sd t2, 88(sp)
    sd s0, 96(sp)
    sd s1, 104(sp)
    sd a0, 112(sp)
    sd a1, 120(sp)
    sd a2, 128(sp)
    sd a3, 136(sp)
    sd a4, 144(sp)
    sd a5, 152(sp)
    sd a6, 160(sp)
    sd a7, 168(sp)
    sd s2, 176(sp)
    sd s3, 184(sp)
    sd s4, 192(sp)
    sd s5, 200(sp)
    sd s6, 208(sp)
    sd s7, 216(sp)
    sd s8, 224(sp)
    sd s9, 232(sp)
    sd s10, 240(sp)
    sd s11, 248(sp)
    sd t3, 256(sp)
    sd t4, 264(sp)
    sd t5, 272(sp)
    sd t6, 280(sp)

    // a0寄存器作为参数传递给kerneltrap
    mv a0, sp

    // 调用C语言中断处理函数
    call kerneltrap

    // 恢复所有通用寄存器
    ld ra, 40(sp)
    ld gp, 56(sp)
    ld tp, 64(sp)
    ld t0, 72(sp)
    ld t1, 80(sp)
    ld t2, 88(sp)
    ld s0, 96(sp)
    ld s1, 104(sp)
    ld a0, 112(sp)
    ld a1, 120(sp)
    ld a2, 128(sp)
    ld a3, 136(sp)
    ld a4, 144(sp)
    ld a5, 152(sp)
    ld a6, 160(sp)
    ld a7, 168(sp)
    ld s2, 176(sp)
    ld s3, 184(sp)
    ld s4, 192(sp)
    ld s5, 200(sp)
    ld s6, 208(sp)
    ld s7, 216(sp)
    ld s8, 224(sp)
    ld s9, 232(sp)
    ld s10, 240(sp)
    ld s11, 248(sp)
    ld t3, 256(sp)
    ld t4, 264(sp)
    ld t5, 272(sp)
    ld t6, 280(sp)
    csrrw sp, sscratch, sp

    // 从中断返回
    sret
