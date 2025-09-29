#include "types.h"
#include "memlayout.h"
#include "global_func.h"
#include "list.h"

extern char end[]; // 由链接器定义, 指向内核数据段的末尾

// 空闲块的链表节点直接使用块自身的内存(侵入式链表)
// 链表是双向循环链表 (list.c/list.h)

// 简易的自旋锁占位符, 当前版本是空实现, 不支持多核并发
struct spinlock
{
  int dummy;
};
static inline void initlock(struct spinlock *lk, const char *name)
{
  (void)lk;
  (void)name;
}
static inline void acquire(struct spinlock *lk) { (void)lk; }
static inline void release(struct spinlock *lk) { (void)lk; }


static int nsizes; // 块大小的种类数量 (k=0..nsizes-1)

#define LEAF_SIZE 128                                    // 最小块大小, 16字节
#define MAXSIZE (nsizes - 1)                             // 最大块的阶
#define BLK_SIZE(k) ((1L << (k)) * LEAF_SIZE)            // 第k阶块的大小
#define HEAP_SIZE BLK_SIZE(MAXSIZE)                      // 整个堆的大小
#define NBLK(k) (1 << (MAXSIZE - (k)))                   // 第k阶块的总数量
#define ROUNDUP(n, sz) (((((n) - 1) / (sz)) + 1) * (sz)) // 向上取整

// 描述每一阶(size)信息的结构体
struct sz_info
{
  struct list free; // 这一阶的空闲块链表 (哨兵头节点)
  char *alloc;      // XOR-pair压缩的分配位图 (每对伙伴一个bit)
  char *split;      // 块分裂位图 (标记父块是否已分裂)
};
typedef struct sz_info Sz_info;

static Sz_info *bd_sizes;       // 指向所有阶信息的数组
static void *bd_base;           // 伙伴系统管理的内存区域的基地址
static struct spinlock bd_lock; // 保护伙伴系统的锁
static uint64 freelist_bitmap;  // 新增: 用于快速查找非空闲链表的位图

// ===== 位操作辅助函数 =====

#define bit_isset(array, index) ((((char *)(array))[(index) / 8] & (1 << ((index) % 8))) != 0)
#define bit_isset_pair(array, index) (bit_isset((array), (index) / 2))

#define bit_set(array, index) do { \
    ((char *)(array))[(index) / 8] |= (1 << ((index) % 8)); \
} while(0)

#define bit_xor_pair(array, index) do { \
    ((char *)(array))[((index) / 2) / 8] ^= (1 << (((index) / 2) % 8)); \
} while(0)

#define bit_clear(array, index) do { \
    ((char *)(array))[(index) / 8] &= ~(1 << ((index) % 8)); \
} while(0)

// ===== 地址与索引计算辅助函数 =====

// 计算能容纳n字节所需的最小块的阶k
static int firstk(uint64 n)
{
  int k = 0;
  uint64 size = LEAF_SIZE;
  while (size < n)
  {
    k++;
    size <<= 1;
  }
  return k;
}

// 设置freelist_bitmap中对应阶的位
static inline void set_freelist_bit(int k) {
  freelist_bitmap |= (1L << k);
}

// 清除freelist_bitmap中对应阶的位
static inline void clear_freelist_bit(int k) {
  freelist_bitmap &= ~(1L << k);
}

// 查找大于等于k的第一个置位。返回bit的位置(从1开始), 未找到则返回0。
static inline int find_first_set_ge(uint64 mask, int k) {
    for (int i = k; i < 64; i++) {
        if ((mask >> i) & 1) {
            return i + 1;
        }
    }
    return 0;
}

// 根据指针p计算其在k阶块中的索引
static int blk_index(int k, char *p)
{
  int n = p - (char *)bd_base;
  return n / BLK_SIZE(k);
}
// 根据k阶块的索引bi计算其起始地址
static void *addr(int k, int bi)
{
  int n = bi * BLK_SIZE(k);
  return (char *)bd_base + n;
}
// 计算指针p所在的下一个k阶块的索引 (用于标记范围)
static int blk_index_next(int k, char *p)
{
  int n = (p - (char *)bd_base) / BLK_SIZE(k);
  if ((p - (char *)bd_base) % BLK_SIZE(k))
    n++;
  return n;
}
// 计算n的log2 (向上取整)
static int log2u(uint64 n)
{
  int k = 0;
  while (n > 1)
  {
    k++;
    n >>= 1;
  }
  return k;
}

// ===== 调试打印辅助函数 =====

// 打印位图向量, 将连续的0或1区间合并显示
static void bd_print_vector(char *vector, int len)
{
  int last = 1, lb = 0; // 假设初始状态为1, 以便打印第一个0区间
  for (int b = 0; b < len; b++)
  {
    int cur = bit_isset(vector, b);
    if (cur == last)
      continue;
    if (last == 1)
      printf(" [%d, %d)", lb, b); // 状态从1变为0, 打印一个已分配(或split)的区间
    lb = b;
    last = cur;
  }
  if (lb == 0 || last == 1)
    printf(" [%d, %d)", lb, len); // 打印最后一个区间
  printf("\n");
}

// 打印整个伙伴系统的状态
void bd_print()
{
  for (int k = 0; k < nsizes; k++)
  {
    printf("bd: size %d (blksz %d nblk %d): free list:", k, BLK_SIZE(k), NBLK(k));
    lst_print(&bd_sizes[k].free);
    printf("  alloc:");
    bd_print_vector(bd_sizes[k].alloc, NBLK(k));
    if (k > 0)
    {
      printf("  split:");
      bd_print_vector(bd_sizes[k].split, NBLK(k));
    }
  }
}

// ===== 核心初始化与分配/释放逻辑 =====

// 将[start, stop)地址范围内的所有块标记为已分配
static void bd_mark(void *start, void *stop)
{
  int bi, bj;
  if (((uint64)start % LEAF_SIZE) || ((uint64)stop % LEAF_SIZE))
    return; // 地址必须对齐, 简化处理
  for (int k = 0; k < nsizes; k++)
  {
    bi = blk_index(k, start);
    bj = blk_index_next(k, stop);
    for (; bi < bj; bi++)
    {
      if (k > 0)
        bit_set(bd_sizes[k].split, bi);   // 标记为已分裂, 防止被上层合并
      bit_xor_pair(bd_sizes[k].alloc, bi); // 翻转XOR位, 标记为已分配
    }
  }
}

// 初始化时, 处理一对伙伴块, 将未被标记的块加入空闲链表
static int bd_initfree_pair(int k, int bi, void *min_left, void *max_right)
{
  int buddy = (bi % 2) == 0 ? bi + 1 : bi - 1;
  int free = 0;
  if (bit_isset_pair(bd_sizes[k].alloc, bi)) // 如果XOR位为1, 说明这对伙伴中有一个是空闲的
  {
    free = BLK_SIZE(k);
    // 检查伙伴块是否在有效内存范围内
    if (addr(k, buddy) >= min_left && addr(k, buddy) < max_right) {
      if(lst_empty(&bd_sizes[k].free)) set_freelist_bit(k);
      lst_push(&bd_sizes[k].free, addr(k, buddy)); // 将伙伴块加入空闲链表
    } else {
      if(lst_empty(&bd_sizes[k].free)) set_freelist_bit(k);
      lst_push(&bd_sizes[k].free, addr(k, bi)); // 否则将当前块加入
    }
  }
  return free;
}

// 初始化空闲链表
static int bd_initfree(void *bd_left, void *bd_right, void *min_left, void *max_right)
{
  int free = 0;
  for (int k = 0; k < MAXSIZE; k++)
  {
    int left = blk_index_next(k, bd_left);
    int right = blk_index(k, bd_right);
    free += bd_initfree_pair(k, left, min_left, max_right);
    if (right <= left)
      continue;
    free += bd_initfree_pair(k, right, min_left, max_right);
  }
  return free;
}

// 标记伙伴系统自身元数据所占用的空间
static int bd_mark_data_structures(char *p)
{
  int meta = p - (char *)bd_base;//元数据大小
  bd_mark(bd_base, p);
  return meta;
}

// 标记物理内存末端不可用的部分
static int bd_mark_unavailable(void *end)
{
  // 计算总管理空间与实际物理内存顶端之间的差值
  int unavailable = BLK_SIZE(MAXSIZE) - (end - bd_base);
  if (unavailable > 0)
    unavailable = ROUNDUP(unavailable, LEAF_SIZE);
  printf("bd: 0x%x bytes unavailable\n", unavailable);
  // 标记这部分为已分配
  void *bd_end = bd_base + BLK_SIZE(MAXSIZE) - unavailable;
  bd_mark(bd_end, bd_base + BLK_SIZE(MAXSIZE));
  return unavailable;
}

// 初始化整个伙伴分配器
static void bd_init(void *base, void *end)
{
  char *p = (char *)ROUNDUP((uint64)base, LEAF_SIZE); // 对齐我们的元数据起始地址
  int sz;
  initlock(&bd_lock, "buddy");
  freelist_bitmap = 0; // 初始化位图为0
  bd_base = (void *)p; // 设置内存池基地址

  // 计算需要的阶数
  nsizes = log2u(((char *)end - p) / LEAF_SIZE) + 1;
  if (((char *)end - p) > BLK_SIZE(MAXSIZE))
    nsizes++; // 向上取整确保覆盖所有内存

  printf("bd: memory sz is %d bytes; allocate size array length %d\n", (int)((char *)end - p), nsizes);

  // 分配元数据空间: Sz_info数组, alloc位图, split位图
  bd_sizes = (Sz_info *)p;
  p += sizeof(Sz_info) * nsizes;
  memset(bd_sizes, 0, sizeof(Sz_info) * nsizes);
  for (int k = 0; k < nsizes; k++)
  {
    lst_init(&bd_sizes[k].free);
    // 为alloc位图分配空间, 16位对齐
    //对于第 k 阶，总共有 NBLK(k) 个块。
    //因为每对伙伴共享一个比特位，所以需要 NBLK(k) / 2 个比特位。
    //将所需比特位数转换为字节数，需要 ceil( (NBLK(k) / 2) / 8 ) 个字节。
    sz = sizeof(char) * ROUNDUP(NBLK(k), 16) / 16;
    bd_sizes[k].alloc = p;
    memset(bd_sizes[k].alloc, 0, sz);
    p += sz;
  }
  for (int k = 1; k < nsizes; k++)
  {
    // 为split位图分配空间, 8位对齐
    sz = sizeof(char) * (ROUNDUP(NBLK(k), 8)) / 8;
    bd_sizes[k].split = p;
    memset(bd_sizes[k].split, 0, sz);
    p += sz;
  }
  // 对齐元数据末尾
  p = (char *)ROUNDUP((uint64)p, LEAF_SIZE); 

  // 标记元数据和不可用内存区域为已分配
  int meta = bd_mark_data_structures(p);
  int unavailable = bd_mark_unavailable(end);

  // 初始化空闲链表
  void *bd_end = bd_base + BLK_SIZE(MAXSIZE) - unavailable;
  int free = bd_initfree(p, bd_end, p, end);

  // 检查计算出的空闲内存是否与实际相符
  if (free != BLK_SIZE(MAXSIZE) - meta - unavailable)
  {
    printf("bd: free mismatch %d vs %d\n", free, BLK_SIZE(MAXSIZE) - meta - unavailable);
    panic("bd_init: free mem");
  }
}

// 分配nbytes字节的内存
void *kmalloc(uint64 nbytes)
{
  int fk, k;

  // 最小分配LEAF_SIZE
  if (nbytes < LEAF_SIZE)
    nbytes = LEAF_SIZE;

  acquire(&bd_lock);

  // 1. 找到能满足nbytes的最小的阶fk
  fk = firstk(nbytes);

  // 2. 使用位图快速查找有空闲块的最小的阶k (k >= fk)
  int bit = find_first_set_ge(freelist_bitmap, fk);
  k = (bit > 0) ? (bit - 1) : nsizes;

  if (k >= nsizes)
  { // 没有找到足够大的空闲块
    release(&bd_lock);
    return 0;
  }

  // 3. 从k阶的空闲链表中取出一个块
  char *p = (char *)lst_pop(&bd_sizes[k].free);
  if(lst_empty(&bd_sizes[k].free)) clear_freelist_bit(k); // 更新位图
  bit_xor_pair(bd_sizes[k].alloc, blk_index(k, p)); // 标记为已分配

  // 4. 如果k > fk, 需要将大块分裂
  for (; k > fk; k--)
  {
    // 将块p分裂成两半, 前半部分仍是p, 后半部分是q
    char *q = p + BLK_SIZE(k - 1);
    bit_set(bd_sizes[k].split, blk_index(k, p));             // 标记父块已分裂
    bit_xor_pair(bd_sizes[k - 1].alloc, blk_index(k - 1, p)); // 标记p为已分配
    if(lst_empty(&bd_sizes[k-1].free)) set_freelist_bit(k-1); // 更新位图
    lst_push(&bd_sizes[k - 1].free, q);                      // 将后半部分q加入低一阶的空闲链表
  }

  release(&bd_lock);
  return p; // 返回分配到的内存地址
}

// 确定指针p所在块的阶
// 向上查找, 直到找到一个未分裂的父块
static int size_of_block(char *p)
{
  for (int k = 0; k < nsizes; k++)
  {
    // 如果k+1阶的块是分裂的, 说明p属于k阶块
    if (bit_isset(bd_sizes[k + 1].split, blk_index(k + 1, p)))
      return k;
  }
  return 0;
}

// 释放内存
void free_page(void *vp)
{
  void *q;
  int k;
  char *p = (char *)vp;

  acquire(&bd_lock);

  // 1. 确定要释放的块p的阶k
  // 2. 循环向上合并
  for (k = size_of_block(p); k < MAXSIZE; k++)
  {
    int bi = blk_index(k, p);
    int buddy = (bi % 2) == 0 ? bi + 1 : bi - 1; // 计算伙伴块的索引

    bit_xor_pair(bd_sizes[k].alloc, bi); // 翻转XOR位, 标记p为"空闲"

    // 检查伙伴块是否也空闲 (如果XOR位为1, 说明伙伴块是已分配的)
    if (bit_isset_pair(bd_sizes[k].alloc, buddy))
      break; // 伙伴块已分配, 停止合并

    // 3. 伙伴块空闲, 进行合并
    q = addr(k, buddy);           // 获取伙伴块的地址
    lst_remove((struct list *)q); // 从空闲链表中移除伙伴块
    if(lst_empty(&bd_sizes[k].free)) clear_freelist_bit(k); // 更新位图

    // 选择地址较小的块作为合并后大块的基地址
    if (buddy % 2 == 0)
      p = q;
    // 清除父块的分裂标记
    bit_clear(bd_sizes[k + 1].split, blk_index(k + 1, p));
  }

  // 4. 将最终合并的块或未合并的块加入对应阶的空闲链表
  if(lst_empty(&bd_sizes[k].free)) set_freelist_bit(k); // 更新位图
  lst_push(&bd_sizes[k].free, p);
  release(&bd_lock);
}

// ===== 兼容旧接口的包装函数 =====

static int initialized = 0; // 初始化标志, 防止重复初始化
// 确保物理内存管理器只被初始化一次
void pmm_init()
{
  if (!initialized)
  {
    uint64 start = PGROUNDUP((uint64)end);   // 从内核末尾对齐的地址开始
    bd_init((void *)start, (void *)PHYSTOP); // 初始化伙伴系统
    initialized = 1;
  }
}

// 分配单个物理页
void *alloc_page()
{
  return kmalloc(PGSIZE);
}

// 分配2^order个连续的物理页
void *alloc_pages(int count)
{
  return kmalloc(count * PGSIZE);
}

// 释放由kmalloc分配的内存
void kfree(void *p)
{
  free_page(p);
}
