// 虚拟内存管理 (vm.c)

#include "memlayout.h"
#include "paging.h"
#include "global_func.h"
#include "types.h"

// 声明外部函数和变量
void* alloc_page(void);
void free_page(void*);
void* memset(void*, int, uint);
extern char etext[]; // 内核代码段结束地址

// 全局唯一的内核页表
pagetable_t kernel_pagetable;

// 从最顶级页表开始，在页表中查找虚拟地址va对应的PTE地址
// 若alloc为1, 则在PTE无效时分配新页
static pte_t*
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= PHYSTOP)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[VPN(va, level)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {//无效
      if(!alloc || (pagetable = (pagetable_t)alloc_page()) == 0) //不分配或者分配失败的情形
        return 0;
      memset(pagetable, 0, PGSIZE);//否则分配成功，清零
      *pte = PA2PTE(pagetable) | PTE_V;//写入pte，但是不会写入最后一级的pte
    }
  }
  return &pagetable[VPN(va, 0)];
}

// 创建一段虚拟地址到物理地址的映射
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){//为[a, last]区间内的每一页建立映射
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)//已经存在映射，这是不应该的
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;//写入最后一级的pte
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 创建内核页表
void
kvm_init()
{
  kernel_pagetable = (pagetable_t) alloc_page();
  memset(kernel_pagetable, 0, PGSIZE);

  // 映射UART设备
  mappages(kernel_pagetable, UART0, PGSIZE, UART0, PTE_R | PTE_W);

  // 映射内核代码段 (R+X)
  mappages(kernel_pagetable, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R | PTE_X);

  // 映射内核数据段和剩余物理内存 (R+W)
  mappages(kernel_pagetable, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R | PTE_W);
}

// 启用分页 (加载内核页表到SATP寄存器)
void
kvm_init_hart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}
