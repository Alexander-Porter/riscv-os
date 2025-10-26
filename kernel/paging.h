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
#define PX(level, va) VPN((va), (level))

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
#define PTE_A (1L << 6) // Accessed
#define PTE_D (1L << 7) // Dirty
#define PTE_COW (1L << 8) // Copy-on-write 标志 (软件定义)
#define PTE_SHARED (1L << 9) // 共享内存标志 (软件定义)
#define PTE_FLAGS(pte)  ((pte) & 0x3FF)


// -------------------- SATP 寄存器 -------------------- 

// Supervisor Address Translation and Protection (SATP) 寄存器
// +----16----+----4----+-----------------44-----------------+
// | 63..60   | 59..44 |              43..0                 |
// |   MODE   |  ASID  |                PPN                 |
// +----------+--------+------------------------------------+
#define SATP_SV39 (8L << 60) // MODE=8 表示Sv39分页模式
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// -------------------- CSR 读写宏 -------------------- 

// 读写SATP寄存器的内联汇编宏
static inline void w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64 r_satp() {
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x));
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

// 用户态页表相关操作
uint64 walkaddr(pagetable_t pagetable, uint64 va);
pagetable_t uvmcreate(void);
void uvminit(pagetable_t pagetable, uchar *src, int sz);
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int perm);
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);
void uvmfree(pagetable_t pagetable, uint64 sz);
int uvmcopy(pagetable_t old, pagetable_t newp, uint64 sz);
void uvmclear(pagetable_t pagetable, uint64 va);
int copyout(pagetable_t pagetable, uint64 dstva, void *src, uint64 len);
int copyin(pagetable_t pagetable, void *dst, uint64 srcva, uint64 len);
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
int cow_allocpage(pagetable_t pagetable, uint64 va);


#endif // __PAGING_H
