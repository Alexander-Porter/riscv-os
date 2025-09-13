#ifndef __SBI_H
#define __SBI_H

#include "types.h"

// RISC-V SBI 调用封装

// SBI扩展ID (EID)
#define SBI_EID_TIME 0x54494D45 // Timer Extension

// SBI函数ID (FID)
#define SBI_FID_SET_TIMER 0

// 通用的SBI调用函数
static inline uint64 sbi_call(uint64 eid, uint64 fid, uint64 arg0, uint64 arg1, uint64 arg2) {
    uint64 ret;
    register uint64 a0 asm("a0") = arg0;
    register uint64 a1 asm("a1") = arg1;
    register uint64 a2 asm("a2") = arg2;
    register uint64 a6 asm("a6") = fid;
    register uint64 a7 asm("a7") = eid;

    asm volatile(
        "ecall"
        : "=r"(a0)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a6), "r"(a7)
        : "memory"
    );
    ret = a0;
    return ret;
}

// 设置下一次时钟中断
static inline void sbi_set_timer(uint64 time) {
    sbi_call(SBI_EID_TIME, SBI_FID_SET_TIMER, time, 0, 0);
}

#endif // __SBI_H