// 虚拟内存管理 (vm.c)

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "paging.h"
#include "global_func.h"
#include "proc.h"
#include "include/riscv.h"

extern char etext[];
extern char trampoline[];

pagetable_t kernel_pagetable;

static pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk");

  for (int level = PT_LEVELS - 1; level > 0; level--)
  {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V)
    {
      pagetable = (pagetable_t)PTE2PA(*pte);
    }
    else
    {
      if (!alloc)
        return 0;
      pagetable = (pagetable_t)alloc_page();
      if (pagetable == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

uint64 walkaddr(pagetable_t pagetable, uint64 va)
{
  if (va >= MAXVA)
    return 0;

  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  return PTE2PA(*pte);
}

int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  if (size == 0)
    panic("mappages: size");

  if (va % PGSIZE || pa % PGSIZE || size % PGSIZE)
    panic("mappages: alignment");

  uint64 last = va + size - PGSIZE;
  for (uint64 a = va; ; a += PGSIZE, pa += PGSIZE)
  {
    pte_t *pte = walk(pagetable, a, 1);
    if (pte == 0)
      return -1;
    if (*pte & PTE_V) {
      struct proc *mp = myproc();
      void *ra = __builtin_return_address(0);
      printf("mappages remap: pt=%p va=0x%lx pa=0x%lx ra=%p proc=%p name=%s\n",
        pagetable, a, pa, ra, mp, mp ? mp->name : "<none>");
      panic("mappages: remap");
    }
    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last)
      break;
  }
  return 0;
}

// 查询某虚拟地址是否已有有效PTE（不要求PTE_U），用于诸如守护页等判断
int pte_is_valid(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  return pte && (*pte & PTE_V);
}

static void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

void kvm_init(void)
{
  kernel_pagetable = (pagetable_t)alloc_page();
  if (kernel_pagetable == 0)
    panic("kvm_init: alloc_page");
  memset(kernel_pagetable, 0, PGSIZE);

  kvmmap(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  kvmmap(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  kvmmap(kernel_pagetable, PLIC, PLIC, PLIC_SIZE, PTE_R | PTE_W);
  kvmmap(kernel_pagetable, CLINT, CLINT, CLINT_SIZE, PTE_R | PTE_W);

  uint64 code_sz = PGROUNDUP((uint64)etext - KERNBASE);
  kvmmap(kernel_pagetable, KERNBASE, KERNBASE, code_sz, PTE_R | PTE_X);

  uint64 data_start = PGROUNDUP((uint64)etext);
  if (PHYSTOP > data_start)
    kvmmap(kernel_pagetable, data_start, data_start, PHYSTOP - data_start, PTE_R | PTE_W);

  kvmmap(kernel_pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  proc_mapstacks(kernel_pagetable);
}

void kvm_init_hart(void)
{
  sfence_vma();
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

pagetable_t uvmcreate(void)
{
  pagetable_t pagetable = (pagetable_t)alloc_page();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

void uvminit(pagetable_t pagetable, uchar *src, int sz)
{
  if (sz > PGSIZE)
    panic("uvminit");

  char *mem = alloc_page();
  if (mem == 0)
    panic("uvminit: alloc_page");
  memset(mem, 0, PGSIZE);
  memmove(mem, src, sz);
  if (mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_R | PTE_W | PTE_X | PTE_U) != 0)
    panic("uvminit: mappages");
}

uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int perm)
{
  if (newsz < oldsz)
    return oldsz;

  uint64 a = PGROUNDUP(oldsz);
  for (; a < newsz; a += PGSIZE)
  {
    char *mem = alloc_page();
    if (mem == 0)
    {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
  if (mappages(pagetable, a, PGSIZE, (uint64)mem, perm) != 0)
    {
      free_page(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if (newsz >= oldsz)
    return oldsz;

  uint64 new_aligned = PGROUNDUP(newsz);
  uint64 old_aligned = PGROUNDUP(oldsz);
  if (new_aligned < old_aligned)
  {
    uint64 npages = (old_aligned - new_aligned) / PGSIZE;
    uvmunmap(pagetable, new_aligned, npages, 1);
  }
  return newsz;
}

void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  if (va % PGSIZE)
    panic("uvmunmap: not aligned");

  for (uint64 a = va; a < va + npages * PGSIZE; a += PGSIZE)
  {
    pte_t *pte = walk(pagetable, a, 0);
    if (pte == 0)
      continue;
    if ((*pte & PTE_V) == 0)
      continue;
    if (do_free)
    {
      uint64 pa = PTE2PA(*pte);
      free_page((void *)pa);
    }
    *pte = 0;
  }
}

void uvmfree(pagetable_t pagetable, uint64 sz)
{
  if (sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  destroy_pagetable(pagetable);
}

int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  uint64 i;
  int need_sfence = 0;
  for (i = 0; i < sz; i += PGSIZE)
  {
    pte_t *pte = walk(old, i, 0);
    if (pte == 0)
      continue;
    if ((*pte & PTE_V) == 0)
      continue;
    uint64 pa = PTE2PA(*pte);
    uint flags = PTE_FLAGS(*pte);
    uint new_flags = flags;

    if (flags & (PTE_W | PTE_COW))
    {
      new_flags = (flags | PTE_COW) & ~PTE_W;
      need_sfence = 1;
    }

    if (mappages(new, i, PGSIZE, pa, new_flags) != 0)
      goto err;

    if (new_flags != flags)
      *pte = PA2PTE(pa) | new_flags;

    incref_page((void *)pa);
  }
  if (need_sfence)
    sfence_vma();
  return 0;

err:
  if (i > 0)
    uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

void uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

int copyout(pagetable_t pagetable, uint64 dstva, void *src, uint64 len)
{
  char *s = (char *)src;
  while (len > 0)
  {
    uint64 va0 = PGROUNDDOWN(dstva);
    if (cow_allocpage(pagetable, va0) < 0)
      return -1;
    uint64 pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    uint64 n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), s, n);
    len -= n;
    s += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

int copyin(pagetable_t pagetable, void *dst, uint64 srcva, uint64 len)
{
  char *d = (char *)dst;
  while (len > 0)
  {
    uint64 va0 = PGROUNDDOWN(srcva);
    uint64 pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    uint64 n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;
    memmove(d, (void *)(pa0 + (srcva - va0)), n);
    len -= n;
    d += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n;
  int got_null = 0;

  while (got_null == 0 && max > 0)
  {
    uint64 va0 = PGROUNDDOWN(srcva);
    uint64 pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;
    char *p = (char *)(pa0 + (srcva - va0));
    while (n > 0)
    {
      char c = *p;
      *dst = c;
      dst++;
      p++;
      max--;
      n--;
      if (c == '\0')
      {
        got_null = 1;
        break;
      }
    }
    srcva = va0 + PGSIZE;
  }
  return got_null ? 0 : -1;
}

// 为指定的用户态虚拟地址区间 [va_start, va_end) 分配并建立映射，权限为 perm|PTE_U
// 要求：va_start/va_end 均按页对齐，且 va_start < va_end
// 返回 0 表示成功，-1 表示失败（部分映射失败会做最佳努力回滚）
int uvmalloc_at(pagetable_t pagetable, uint64 va_start, uint64 va_end, int perm)
{
  if ((va_start % PGSIZE) != 0 || (va_end % PGSIZE) != 0 || va_end <= va_start)
    return -1;
  int uperm = (perm | PTE_U);
  uint64 va;
  for (va = va_start; va < va_end; va += PGSIZE)
  {
    char *mem = alloc_page();
    if (mem == 0)
      goto err;
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, va, PGSIZE, (uint64)mem, uperm) != 0)
    {
      free_page(mem);
      goto err;
    }
  }
  return 0;

err:
  // 回滚已映射的页面
  if (va > va_start)
  {
    uint64 mapped_pages = (va - va_start) / PGSIZE;
    uvmunmap(pagetable, va_start, mapped_pages, 1);
  }
  return -1;
}

int cow_allocpage(pagetable_t pagetable, uint64 va)
{
  uint64 va0 = PGROUNDDOWN(va);
  pte_t *pte = walk(pagetable, va0, 0);
  if (pte == 0)
    return -1;
  if ((*pte & PTE_V) == 0)
    return -1;
  if (!(*pte & PTE_COW))
  {
    if ((*pte & PTE_W) == 0)
      return -1; // 非 COW 且不可写，视为非法访问
    return 0;
  }

  uint64 pa = PTE2PA(*pte);
  int ref = pageref((void *)pa);
  uint64 flags = PTE_FLAGS(*pte);

  if (ref <= 1)
  {
    flags = (flags | PTE_W) & ~PTE_COW;
    *pte = PA2PTE(pa) | flags;
    sfence_vma();
    return 0;
  }

  char *mem = alloc_page();
  if (mem == 0)
    return -1;
  memmove(mem, (void *)pa, PGSIZE);
  flags = (flags | PTE_W) & ~PTE_COW;
  *pte = PA2PTE(mem) | flags;
  sfence_vma();
  free_page((void *)pa);
  return 0;
}

static void indent(int level)
{
  for (int i = 0; i < level; i++)
    printf("  ");
}

static const char *perm_str(pte_t pte)
{
  static char buf[8];
  int i = 0;
  buf[i++] = (pte & PTE_R) ? 'R' : '-';
  buf[i++] = (pte & PTE_W) ? 'W' : '-';
  buf[i++] = (pte & PTE_X) ? 'X' : '-';
  buf[i++] = (pte & PTE_U) ? 'U' : '-';
  buf[i] = 0;
  return buf;
}

void dump_pagetable(pagetable_t pt, int level)
{
  if (pt == 0)
  {
    indent(2 - level);
    printf("<null pagetable>\n");
    return;
  }

  if (level < 0 || level > 2)
    return;

  const int SHOW_ENTRIES = 16;
  for (int idx = 0; idx < SHOW_ENTRIES; idx++)
  {
    pte_t pte = pt[idx];
    if (!(pte & PTE_V))
      continue;
    uint64 pa = PTE2PA(pte);
    int is_leaf = (pte & (PTE_R | PTE_W | PTE_X)) != 0;
    indent(2 - level);
    printf("L%d[%d]: PTE=0x%lx PA=0x%lx %s %s\n", level, idx, pte, pa, perm_str(pte), is_leaf ? "(leaf)" : "");
    if (!is_leaf)
      dump_pagetable((pagetable_t)pa, level - 1);
  }
}

void destroy_pagetable(pagetable_t pt)
{
  if (pt == 0)
    return;
  for (int i = 0; i < PT_ENTRIES; i++)
  {
    pte_t p = pt[i];
    if (p & PTE_V)
    {
      if ((p & (PTE_R | PTE_W | PTE_X)) == 0)
      {
        pagetable_t child = (pagetable_t)PTE2PA(p);
        destroy_pagetable(child);
        pt[i] = 0;
      }
      else
      {
        pt[i] = 0;
      }
    }
  }
  free_page(pt);
}
