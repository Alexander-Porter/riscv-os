#ifndef __SBI_H
#define __SBI_H

#include "../types.h"

// SBI 调用号定义
#define SBI_SET_TIMER 0x0
#define SBI_CONSOLE_PUTCHAR 0x1
#define SBI_CONSOLE_GETCHAR 0x2
#define SBI_CLEAR_IPI 0x3
#define SBI_SEND_IPI 0x4
#define SBI_REMOTE_FENCE_I 0x5
#define SBI_REMOTE_SFENCE_VMA 0x6
#define SBI_REMOTE_SFENCE_VMA_ASID 0x7
#define SBI_SHUTDOWN 0x8

// SBI 扩展ID
#define SBI_EXT_0_1_SET_TIMER 0x0
#define SBI_EXT_0_1_CONSOLE_PUTCHAR 0x1
#define SBI_EXT_0_1_CONSOLE_GETCHAR 0x2
#define SBI_EXT_0_1_CLEAR_IPI 0x3
#define SBI_EXT_0_1_SEND_IPI 0x4
#define SBI_EXT_0_1_REMOTE_FENCE_I 0x5
#define SBI_EXT_0_1_REMOTE_SFENCE_VMA 0x6
#define SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID 0x7
#define SBI_EXT_0_1_SHUTDOWN 0x8

#define SBI_EXT_TIME 0x54494D45
#define SBI_EXT_IPI 0x735049
#define SBI_EXT_RFENCE 0x52464E43

// SBI 返回值结构
struct sbiret {
    long error;
    long value;
};

// SBI 调用函数
static inline struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
                                      uint64 arg1, uint64 arg2,
                                      uint64 arg3, uint64 arg4,
                                      uint64 arg5)
{
    struct sbiret ret;
    register uint64 a0 asm ("a0") = (uint64)(arg0);
    register uint64 a1 asm ("a1") = (uint64)(arg1);
    register uint64 a2 asm ("a2") = (uint64)(arg2);
    register uint64 a3 asm ("a3") = (uint64)(arg3);
    register uint64 a4 asm ("a4") = (uint64)(arg4);
    register uint64 a5 asm ("a5") = (uint64)(arg5);
    register uint64 a6 asm ("a6") = (uint64)(fid);
    register uint64 a7 asm ("a7") = (uint64)(ext);
    asm volatile ("ecall"
                  : "+r" (a0), "+r" (a1)
                  : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
                  : "memory");
    ret.error = a0;
    ret.value = a1;
    return ret;
}

// 便捷的SBI调用函数
static inline void sbi_set_timer(uint64 stime_value)
{
    sbi_ecall(SBI_EXT_TIME, 0, stime_value, 0, 0, 0, 0, 0);
}

static inline void sbi_console_putchar(int ch)
{
    sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

static inline int sbi_console_getchar(void)
{
    struct sbiret ret = sbi_ecall(SBI_EXT_0_1_CONSOLE_GETCHAR, 0, 0, 0, 0, 0, 0, 0);
    return ret.error;
}

static inline void sbi_shutdown(void)
{
    sbi_ecall(SBI_EXT_0_1_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);
}

#endif