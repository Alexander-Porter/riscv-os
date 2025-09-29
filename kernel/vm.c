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

  // 映射内核数据段，但故意留出一个未映射的区域用于测试页故障处理
  // 映射从etext到0x85000000的区域
  uint64 data_end = 0x85000000;  // 故意不映射0x85000000之后的区域
  mappages(kernel_pagetable, (uint64)etext, data_end-(uint64)etext, (uint64)etext, PTE_R | PTE_W);
  

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
