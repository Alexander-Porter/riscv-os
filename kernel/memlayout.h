#ifndef __MEMLAYOUT_H
#define __MEMLAYOUT_H

// 内核内存布局定义

#define KERNBASE 0x80000000L                 // 内核基地址
#define PHYSTOP (KERNBASE + 128*1024*1024) // 物理内存最高地址

#ifndef MAXVA
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#endif

#define PGSIZE 4096 // 页大小 (4KB)
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1)) // 向上取整到页边界
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))   // 向下取整到页边界

// QEMU中virt主机的UART设备地址
#define UART0 0x10000000L

// trampoline 与内核栈布局，参考 xv6
#define TRAMPOLINE (MAXVA - PGSIZE)
#define TRAPFRAME  (TRAMPOLINE - PGSIZE)
#define KSTACK(i)  (TRAMPOLINE - ((i) + 1) * 2 * PGSIZE)

#endif // __MEMLAYOUT_H
