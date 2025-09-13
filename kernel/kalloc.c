// 物理内存分配器 (kalloc.c)
// 受xv6启发。当前为非线程安全版本。

#include "types.h"
#include "memlayout.h"
#include "global_func.h"

extern char end[]; // 由链接脚本提供, 指向内核末尾

struct run {
  struct run *next;
};

struct {
  // struct spinlock lock; // TODO: 添加锁以实现线程安全
  struct run *freelist;
} kmem;

// 释放一个物理内存页
void
free_page(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("free_page");

  r = (struct run*)pa;

  // TODO: acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  // TODO: release(&kmem.lock);
}

// 初始化物理内存分配器
// 将从内核末尾到PHYSTOP的内存逐页加入空闲链表
void
pmm_init()
{
  // TODO: initlock(&kmem.lock, "kmem");
  char *p;
  p = (char*)PGROUNDUP((uint64)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)
    free_page(p);
}

// 分配一个4096字节的物理内存页
// 返回页对齐的指针, 若无可用内存则返回0
void*
alloc_page(void)
{
  struct run *r;

  // TODO: acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  // TODO: release(&kmem.lock);

  return (void*)r;
}