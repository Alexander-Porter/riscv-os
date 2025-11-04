// 块缓存实现：缓存磁盘块，LRU替换，支持异步I/O

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "global_func.h"
#include "virtio.h"

// 全局块缓存：64个缓冲区，LRU双向链表
struct {
    struct spinlock lock;
    struct buf buf[NBUF];
    struct buf head;  // 哨兵节点：head.next最新，head.prev最旧
} bcache;

static struct buf *bget(uint dev, uint blockno);

// 性能统计
int buffer_cache_hits = 0;
int buffer_cache_misses = 0;
int disk_read_count = 0;
int disk_write_count = 0;

// binit: 初始化块缓存，在main()中启动时调用
void binit(void)
{
    initlock(&bcache.lock, "bcache");

    // 创建空的循环链表
    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;

    // 所有缓冲区加入链表
    for (struct buf *b = bcache.buf; b < bcache.buf + NBUF; b++)
    {
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        initsleeplock(&b->lock, "buffer");
        bcache.head.next->prev = b;
        bcache.head.next = b;
        b->refcnt = 0;
        b->valid = 0;
    }
}

// bget: 获取块的缓冲区，先查缓存再LRU替换
static struct buf *bget(uint dev, uint blockno)
{
    acquire(&bcache.lock);

    // 先在缓存中找
    for (struct buf *b = bcache.head.next; b != &bcache.head; b = b->next)
    {
        if (b->dev == dev && b->blockno == blockno)
        {
            b->refcnt++;
            buffer_cache_hits++;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // 没找到，从尾部(LRU)找空闲的
    for (struct buf *b = bcache.head.prev; b != &bcache.head; b = b->prev)
    {
        if (b->refcnt == 0)
        {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            buffer_cache_misses++;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    panic("bget: no buffers");
    return 0;
}

// bread: 读块，如果缓存没有就从磁盘读
struct buf *bread(uint dev, uint blockno)
{
    struct buf *b = bget(dev, blockno);
    if (!b->valid)
    {
        virtio_disk_rw(b, 0);
        b->valid = 1;
        disk_read_count++;
    }
    return b;
}

// bwrite: 同步写块，阻塞等待完成
void bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
    disk_write_count++;
}

// bsubmit_write: 异步写块，不等待完成
void bsubmit_write(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bsubmit_write");
    virtio_disk_submit(b, 1);
    disk_write_count++;
}

// bwait: 等待异步I/O完成
void bwait(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwait");
    virtio_disk_wait(b);
}

// brelse: 释放缓冲区，refcnt=0时移到链表头(标记最近使用)
void brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    acquire(&bcache.lock);
    b->refcnt--;
    
    if (b->refcnt == 0)
    {
        // 从当前位置移除
        b->next->prev = b->prev;
        b->prev->next = b->next;
        // 插入链表头(最近使用位置)
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }
    release(&bcache.lock);
}

// bpin: 固定缓冲区，防止被LRU替换(日志系统用)
void bpin(struct buf *b)
{
    acquire(&bcache.lock);
    b->refcnt++;
    release(&bcache.lock);
}

// bunpin: 取消固定
void bunpin(struct buf *b)
{
    acquire(&bcache.lock);
    if (b->refcnt <= 0)
        panic("bunpin");
    b->refcnt--;
    release(&bcache.lock);
}
