#ifndef __MEMLAYOUT_H
#define __MEMLAYOUT_H

#include "param.h"

// 内核内存布局定义

#define KERNBASE 0x80000000L                 // 内核基地址
#define PHYSTOP (KERNBASE + 128*1024*1024) // 物理内存最高地址

#ifndef MAXVA
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#endif

#define PGSIZE 4096 // 页大小 (4KB)
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1)) // 向上取整到页边界
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))   // 向下取整到页边界

// QEMU virt 平台外设 MMIO 地址
#define UART0   0x10000000L
#define UART0_IRQ 10
#ifndef VIRTIO0
#define VIRTIO0 0x10001000L
#endif
#define VIRTIO0_IRQ 1
#define CLINT   0x02000000L
#define CLINT_SIZE 0x000c0000L
#define PLIC    0x0c000000L
#define PLIC_SIZE 0x00400000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING  (PLIC + 0x1000)
#define PLIC_SENABLE(hart)   (PLIC + 0x2080 + (hart) * 0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart)    (PLIC + 0x201004 + (hart) * 0x2000)

// trampoline 与内核栈布局，参考 xv6
#define TRAMPOLINE (MAXVA - PGSIZE)
#define TRAPFRAME  (TRAMPOLINE - PGSIZE)
#define KSTACK(i)  (TRAMPOLINE - ((i) + 1) * 2 * PGSIZE)

// 共享内存区域：位于所有内核栈下方，向下预留 1 页作为缓冲
#define SHM_MAX_PAGES 64
#define SHM_RESERVED_PAGES (NPROC * 2)
#define SHM_BASE (TRAMPOLINE - (SHM_RESERVED_PAGES + SHM_MAX_PAGES + 1) * PGSIZE)
#define SHM_TOP  (SHM_BASE + SHM_MAX_PAGES * PGSIZE)


#endif // __MEMLAYOUT_H
