// Reference-style Buddy Allocator (with XOR pair-bit optimization) + compatibility wrappers
// Simplified integration for current kernel: provides bd_init/bd_malloc/bd_free
// and keeps alloc_page/free_page/alloc_pages/free_pages/kmalloc/kfree wrappers.
// Uses circular doubly-linked list in list.c for O(1) removal.

#include "types.h"
#include "memlayout.h"
#include "global_func.h"
#include "list.h"

extern char end[];

// free list node layout is struct list (next/prev circular)

// Spinlock placeholder
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

// ===== Buddy core (reference adaptation) =====
static int nsizes; // number of sizes (k=0..nsizes-1)

#define LEAF_SIZE 16
#define MAXSIZE (nsizes - 1)
#define BLK_SIZE(k) ((1L << (k)) * LEAF_SIZE)
#define HEAP_SIZE BLK_SIZE(MAXSIZE)
#define NBLK(k) (1 << (MAXSIZE - (k)))
#define ROUNDUP(n, sz) (((((n) - 1) / (sz)) + 1) * (sz))

struct sz_info
{
  struct list free; // free list head (sentinel)
  char *alloc;  // XOR-pair bit array (compressed alloc info)
  char *split;  // split bits (classic buddy split info)
};
typedef struct sz_info Sz_info;

static Sz_info *bd_sizes;
static void *bd_base; // base address of managed memory
static struct spinlock bd_lock;

// bit helpers
static int bit_isset(char *array, int index)
{
  char b = array[index / 8];
  char m = (1 << (index % 8));
  return (b & m) == m;
}
// XOR-compressed version: one bit for a pair => divide index by 2
static int new_bit_isset(char *array, int index)
{
  index /= 2;
  char b = array[index / 8];
  char m = (1 << (index % 8));
  return (b & m) == m;
}
static void bit_set(char *array, int index)
{
  char b = array[index / 8];
  char m = (1 << (index % 8));
  array[index / 8] = (b | m);
}
static void new_bit_set(char *array, int index)
{
  index /= 2;
  char m = (1 << (index % 8));
  array[index / 8] ^= m;
}
static void bit_clear(char *array, int index)
{
  char b = array[index / 8];
  char m = (1 << (index % 8));
  array[index / 8] = (b & ~m);
}

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
static int blk_index(int k, char *p)
{
  int n = p - (char *)bd_base;
  return n / BLK_SIZE(k);
}
static void *addr(int k, int bi)
{
  int n = bi * BLK_SIZE(k);
  return (char *)bd_base + n;
}
static int blk_index_next(int k, char *p)
{
  int n = (p - (char *)bd_base) / BLK_SIZE(k);
  if ((p - (char *)bd_base) % BLK_SIZE(k))
    n++;
  return n;
}
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

// Print helpers (debug) ----------------------------------
static void bd_print_vector(char *vector, int len){
  int last = 1, lb = 0; // assume initial state 1 -> will print first transition
  for(int b=0; b<len; b++){
    int cur = bit_isset(vector, b);
    if(cur == last) continue;
    if(last == 1) printf(" [%d, %d)", lb, b);
    lb = b; last = cur;
  }
  if(lb == 0 || last == 1) printf(" [%d, %d)", lb, len);
  printf("\n");
}

void bd_print(){
  for(int k=0; k<nsizes; k++){
    printf("bd: size %d (blksz %d nblk %d): free list:", k, BLK_SIZE(k), NBLK(k));
    lst_print(&bd_sizes[k].free);
    printf("  alloc:"); bd_print_vector(bd_sizes[k].alloc, NBLK(k));
    if(k>0){ printf("  split:"); bd_print_vector(bd_sizes[k].split, NBLK(k)); }
  }
}

static void bd_mark(void *start, void *stop)
{
  int bi, bj;
  if (((uint64)start % LEAF_SIZE) || ((uint64)stop % LEAF_SIZE))
    return; // simplified
  for (int k = 0; k < nsizes; k++)
  {
    bi = blk_index(k, start);
    bj = blk_index_next(k, stop);
    for (; bi < bj; bi++)
    {
      if (k > 0)
        bit_set(bd_sizes[k].split, bi);
      new_bit_set(bd_sizes[k].alloc, bi);
    }
  }
}

static int bd_initfree_pair(int k, int bi, void *min_left, void *max_right)
{
  int buddy = (bi % 2) == 0 ? bi + 1 : bi - 1;
  int free = 0;
  if (new_bit_isset(bd_sizes[k].alloc, bi))
  {
    free = BLK_SIZE(k);
    if (addr(k, buddy) >= min_left && addr(k, buddy) < max_right)
      lst_push(&bd_sizes[k].free, addr(k, buddy));
    else
      lst_push(&bd_sizes[k].free, addr(k, bi));
  }
  return free;
}

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

static int bd_mark_data_structures(char *p)
{
  int meta = p - (char *)bd_base;
  bd_mark(bd_base, p);
  return meta;
}
static int bd_mark_unavailable(void *end, void *left)
{
  (void)left; // not used separately in reference
  int unavailable = BLK_SIZE(MAXSIZE) - (end - bd_base);
  if (unavailable > 0)
    unavailable = ROUNDUP(unavailable, LEAF_SIZE);
  printf("bd: 0x%x bytes unavailable\n", unavailable);
  void *bd_end = bd_base + BLK_SIZE(MAXSIZE) - unavailable;
  bd_mark(bd_end, bd_base + BLK_SIZE(MAXSIZE));
  return unavailable;
}

static void bd_init(void *base, void *end)
{
  char *p = (char *)ROUNDUP((uint64)base, LEAF_SIZE);
  int sz;
  initlock(&bd_lock, "buddy");
  bd_base = (void *)p;
  nsizes = log2u(((char *)end - p) / LEAF_SIZE) + 1;
  if (((char *)end - p) > BLK_SIZE(MAXSIZE)) nsizes++; // round up
  printf("bd: memory sz is %d bytes; allocate size array length %d\n", (int)((char*)end - p), nsizes);
  bd_sizes = (Sz_info *)p;
  p += sizeof(Sz_info) * nsizes;
  memset(bd_sizes, 0, sizeof(Sz_info) * nsizes);
  for (int k = 0; k < nsizes; k++){
    lst_init(&bd_sizes[k].free);
    sz = sizeof(char) * ROUNDUP(NBLK(k), 16) / 16;
    bd_sizes[k].alloc = p; memset(bd_sizes[k].alloc, 0, sz); p += sz;
  }
  for (int k = 1; k < nsizes; k++)
  {
    sz = sizeof(char) * (ROUNDUP(NBLK(k), 8)) / 8;
    bd_sizes[k].split = p;
    memset(bd_sizes[k].split, 0, sz);
    p += sz;
  }
  p = (char *)ROUNDUP((uint64)p, LEAF_SIZE);
  int meta = bd_mark_data_structures(p);
  int unavailable = bd_mark_unavailable(end, p);
  void *bd_end = bd_base + BLK_SIZE(MAXSIZE) - unavailable;
  int free = bd_initfree(p, bd_end, p, end);
  if(free != BLK_SIZE(MAXSIZE) - meta - unavailable){
    printf("bd: free mismatch %d vs %d\n", free, BLK_SIZE(MAXSIZE) - meta - unavailable);
    panic("bd_init: free mem");
  }
}

static void *bd_malloc(uint64 nbytes)
{
  int fk, k;
  if (nbytes < LEAF_SIZE)
    nbytes = LEAF_SIZE;
  acquire(&bd_lock);
  fk = firstk(nbytes);
  for (k = fk; k < nsizes; k++)
  {
    if (!lst_empty(&bd_sizes[k].free))
      break;
  }
  if (k >= nsizes)
  {
    release(&bd_lock);
    return 0;
  }
  // pop from circular list head
  if(lst_empty(&bd_sizes[k].free)){ release(&bd_lock); return 0; }
  char *p = (char*)lst_pop(&bd_sizes[k].free);
  new_bit_set(bd_sizes[k].alloc, blk_index(k, p));
  for (; k > fk; k--)
  {
    char *q = p + BLK_SIZE(k - 1);
    bit_set(bd_sizes[k].split, blk_index(k, p));
    new_bit_set(bd_sizes[k - 1].alloc, blk_index(k - 1, p));
    lst_push(&bd_sizes[k - 1].free, q);
  }
  release(&bd_lock);
  return p;
}

static int size_of_block(char *p)
{
  for (int k = 0; k < nsizes; k++)
  {
    if (bit_isset(bd_sizes[k + 1].split, blk_index(k + 1, p)))
      return k;
  }
  return 0;
}

static void bd_free(void *vp)
{
  void *q;
  int k;
  char *p = (char *)vp;
  acquire(&bd_lock);
  for (k = size_of_block(p); k < MAXSIZE; k++)
  {
    int bi = blk_index(k, p);
    int buddy = (bi % 2) == 0 ? bi + 1 : bi - 1;
    new_bit_set(bd_sizes[k].alloc, bi);
    if (new_bit_isset(bd_sizes[k].alloc, buddy))
      break;            // buddy allocated -> stop
    q = addr(k, buddy); // merge candidate; buddy must be on free list
    lst_remove((struct list*)q); // O(1) removal
    if (buddy % 2 == 0) p = q; // choose lower address as merged block base
    bit_clear(bd_sizes[k + 1].split, blk_index(k + 1, p));
  }
  lst_push(&bd_sizes[k].free, p); // insert merged/remaining block
  release(&bd_lock);
}

// ===== Compatibility Wrappers =====
static int initialized = 0;
static void ensure_init()
{
  if (!initialized)
  {
    uint64 start = PGROUNDUP((uint64)end);
    bd_init((void *)start, (void *)PHYSTOP);
    initialized = 1;
  }
}

void pmm_init() { ensure_init(); }
void *alloc_page()
{
  ensure_init();
  return bd_malloc(PGSIZE);
}
void free_page(void *p)
{
  ensure_init();
  bd_free(p);
}
void *alloc_pages(int order)
{
  ensure_init();
  return bd_malloc(((uint64)1 << order) * PGSIZE);
}
void free_pages(void *p, int order)
{
  (void)order;
  ensure_init();
  bd_free(p);
}
void *kmalloc(uint64 size)
{
  return bd_malloc(size);
}
void kfree(void *p)
{
  bd_free(p);
}
