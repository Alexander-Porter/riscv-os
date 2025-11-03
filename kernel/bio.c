#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "global_func.h"
#include "virtio.h"

struct {
    struct spinlock lock;
    struct buf buf[NBUF];
    struct buf head;
} bcache;

static struct buf *bget(uint dev, uint blockno);

// 统计信息：缓冲命中/未命中与磁盘I/O次数
int buffer_cache_hits = 0;
int buffer_cache_misses = 0;
int disk_read_count = 0;
int disk_write_count = 0;

void binit(void)
{
    initlock(&bcache.lock, "bcache");

    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;

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

static struct buf *bget(uint dev, uint blockno)
{
    acquire(&bcache.lock);

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

void bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
    disk_write_count++;
}

// 非阻塞提交：仅提交写请求，不等待完成；需后续调用 bwait
void bsubmit_write(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bsubmit_write");
    virtio_disk_submit(b, 1);
    disk_write_count++;
}

// 等待指定缓冲的 I/O 完成
void bwait(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwait");
    virtio_disk_wait(b);
}

void brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    acquire(&bcache.lock);
    b->refcnt--;
    if (b->refcnt == 0)
    {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }
    release(&bcache.lock);
}

void bpin(struct buf *b)
{
    acquire(&bcache.lock);
    b->refcnt++;
    release(&bcache.lock);
}

void bunpin(struct buf *b)
{
    acquire(&bcache.lock);
    if (b->refcnt <= 0)
        panic("bunpin");
    b->refcnt--;
    release(&bcache.lock);
}
