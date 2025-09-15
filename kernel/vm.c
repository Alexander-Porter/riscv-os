// 虚拟内存管理 (vm.c)

#include "memlayout.h"
#include "paging.h"
#include "types.h"
#include "proc.h"
#include "global_func.h"

// 声明外部函数和变量
void* alloc_page(void);
void free_page(void*);
void* memset(void*, int, uint);
void* memmove(void*, const void*, uint);
extern char etext[]; // 内核代码段结束地址
extern char end[];   // 供范围检查时可选使用

// 全局唯一的内核页表
pagetable_t kernel_pagetable;

// 从最顶级页表开始，在页表中查找虚拟地址va对应的PTE地址
// 若alloc为1, 则在PTE无效时分配新页
static pte_t*
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= PHYSTOP)
    panic("walk");

  for(int level = PT_LEVELS-1; level > 0; level--) {
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

// ---------------- 调试与销毁辅助函数 ----------------

// 打印缩进
static void indent(int level) {
  for(int i=0;i<level;i++) printf("  ");
}

static const char *perm_str(pte_t pte) {
  static char buf[8];
  int i=0;
  buf[i++] = (pte & PTE_R)?'R':'-';
  buf[i++] = (pte & PTE_W)?'W':'-';
  buf[i++] = (pte & PTE_X)?'X':'-';
  buf[i++] = (pte & PTE_U)?'U':'-';
  buf[i] = 0;
  return buf;
}

// 递归遍历打印：level=2 顶层 -> 0 末级
void dump_pagetable(pagetable_t pt, int level) {
  if(pt == 0) {
    indent(2-level); printf("<null pagetable>\n");
    return;
  }
  const int SHOW_ENTRIES = 16; 
  
  if(level < 0 || level > 2) return;
  for(int idx=0; idx<SHOW_ENTRIES; idx++) {
    pte_t pte = pt[idx];
    if(!(pte & PTE_V)) continue; // 跳过无效项
    uint64 pa = PTE2PA(pte);
    int is_leaf = (pte & (PTE_R|PTE_W|PTE_X)) != 0;
    indent(2-level);
    printf("L%d[%d]: PTE=0x%lx PA=0x%lx %s %s\n", level, idx, pte, pa, perm_str(pte), is_leaf?"(leaf)":"");
    if(!is_leaf) {
      dump_pagetable((pagetable_t)pa, level-1);
    }
  }
}

// 递归释放非叶子页表（类似 xv6 的 freewalk）。
void destroy_pagetable(pagetable_t pt) {
  if(pt == 0) return;
  for(int i=0;i<PT_ENTRIES;i++) {
    pte_t p = pt[i];
    if(p & PTE_V) {
      if((p & (PTE_R|PTE_W|PTE_X)) == 0) { // 非叶子
        pagetable_t child = (pagetable_t)PTE2PA(p);
        destroy_pagetable(child);
        pt[i] = 0;
      } else {
        // 叶子映射，回收物理页
        //free_page((void*)PTE2PA(p));
        pt[i] = 0;
      }
    }
  }
  free_page(pt); // 释放当前这一层的页框
}

// 为一个进程创建一个用户页表
// (不包含任何映射)
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // 分配一个物理页作为根页表
  pagetable = (pagetable_t) alloc_page();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// 将初始用户程序(initcode)加载到用户页表的虚拟地址0
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("uvminit: initcode larger than a page");
  // 分配一页物理内存
  mem = alloc_page();
  // 将该页清零
  memset(mem, 0, PGSIZE);
  // 将虚拟地址0映射到刚分配的物理页
  // PTE_U: 用户态可以访问
  // PTE_R|W|X: 可读、可写、可执行
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  // 将initcode的内容拷贝到该物理页
  memmove(mem, src, sz);
}
