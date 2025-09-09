        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (4096)
        li t0, 0x10000000 # UART基地址
        li t1, 'S'
        # 启动标记
        sb t1, 0(t0)
        # 输出字符S表示启动
        la sp, stack0
        # addi sp, sp, 4096 错误，立即数字不能太大
        li t3, 4096 #不能使用t0,这是串口的地址
        add sp, sp, t3
        li t1, 'P'
        sb t1, 0(t0)
        call start
spin:
        j spin