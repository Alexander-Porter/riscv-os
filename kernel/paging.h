#ifndef __PAGING_H
#define __PAGING_H

#include "types.h"

// RISC-V Sv39 虚拟内存系统定义

// -------------------- 地址转换 -------------------- 

// 虚拟地址 -> 物理地址 的转换由页表(page table)完成
// 页表是一个包含 2^9 = 512 个页表项(PTE)的物理页
// pagetable_t 是一个指向页表(一个PTE数组)的指针
typedef uint64 *pagetable_t; // 512个PTE组成的页表
typedef uint64 pte_t; // 单个页表项

// 每级页表的条目数：Sv39 每级 9 位索引 => 2^9 = 512
#define PT_INDEX_BITS 9
#define PT_ENTRIES    (1 << PT_INDEX_BITS)   // 512
#define PT_LEVELS     3                      // Sv39 三级页表: level 2,1,0

extern pagetable_t kernel_pagetable;

// 虚拟地址的构成 (Sv39)
// +--------10--------+--------9---------+--------9---------+--------9---------+--------12--------+
// | 63..39 (ignored) | VPN[2] (9 bits) | VPN[1] (9 bits) | VPN[0] (9 bits) | offset (12 bits)|
// +------------------+-----------------+-----------------+-----------------+------------------+
// VPN: Virtual Page Number (虚拟页号)
#define VPN_SHIFT(level) (12 + 9 * (level)) // level 0, 1, 2
#define VPN(va, level) ((((uint64) (va)) >> VPN_SHIFT(level)) & 0x1FF) //提取低9位

// 物理地址的构成
// +--------12--------+-----------------44-----------------+
// | offset (12 bits)|       PPN (Physical Page Number)     |
// +-----------------+------------------------------------+
// PPN: Physical Page Number (物理页号)
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) ((((uint64)pte) >> 10) << 12)

// 页表项 (PTE) 中的标志位
#define PTE_V (1L << 0) // Valid: 有效位
#define PTE_R (1L << 1) // Read: 可读
#define PTE_W (1L << 2) // Write: 可写
#define PTE_X (1L << 3) // Execute: 可执行
#define PTE_U (1L << 4) // User: 用户态可访问


// -------------------- SATP 寄存器 -------------------- 

// Supervisor Address Translation and Protection (SATP) 寄存器
#define SATP_SV39 (8L << 60) // MODE=8 表示Sv39分页模式
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// -------------------- SSTATUS/SIE/SIP 等寄存器 -------------------- 
#define SSTATUS_SIE (1L << 1) // Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9)    // Supervisor External Interrupt Enable
#define SIE_STIE (1L << 5)    // Supervisor Timer Interrupt Enable
#define SIE_SSIE (1L << 1)    // Supervisor Software Interrupt Enable


// -------------------- CSR 读写函数 -------------------- 

static inline uint64 r_sstatus() {
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x));
  return x;
}

static inline void w_sstatus(uint64 x) {
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

static inline void w_stvec(uint64 x) {
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64 r_sie() {
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x));
  return x;
}

static inline void w_sie(uint64 x) {
  asm volatile("csrw sie, %0" : : "r" (x));
}

static inline uint64 r_scause() {
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x));
  return x;
}

static inline uint64 r_sepc() {
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x));
  return x;
}

static inline void w_mideleg(uint64 x) {
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

static inline void w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline void w_sscratch(uint64 x) {
  asm volatile("csrw sscratch, %0" : : "r" (x));
}

static inline uint64 r_time() {
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x));
  return x;
}

// 刷新TLB的宏
static inline void sfence_vma() {
  // a zero rs1 means flush all entries.
  asm volatile("sfence.vma zero, zero");
}

// 调试功能：递归打印页表结构
void dump_pagetable(pagetable_t pt, int level);
// 递归释放页表层级（不释放叶子映射的物理页本身）
void destroy_pagetable(pagetable_t pt);


#endif // __PAGING_H
