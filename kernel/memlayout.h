#ifndef __MEMLAYOUT_H
#define __MEMLAYOUT_H

// 内核内存布局定义

#define KERNBASE 0x80000000L                 // 内核基地址
#define PHYSTOP (KERNBASE + 128*1024*1024) // 物理内存最高地址

#define PGSIZE 4096 // 页大小 (4KB)
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1)) // 向上取整到页边界
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))   // 向下取整到页边界

// QEMU中virt主机的UART设备地址
#define UART0 0x10000000L

#endif // __MEMLAYOUT_H
