#ifndef __BUF_H
#define __BUF_H

#include "sleeplock.h"

#define BSIZE 1024 // 块大小，保持与 xv6 一致

struct buf {
    int valid;             // 数据是否有效
    int disk;              // 是否在磁盘传输中
    uint dev;              // 设备号
    uint blockno;          // 块号
    struct sleeplock lock; // 睡眠锁保护数据区
    uint refcnt;           // 引用计数
    struct buf *prev;      // LRU 双向链表指针
    struct buf *next;
    uchar data[BSIZE];     // 实际数据缓冲区
};

void binit(void);
struct buf *bread(uint dev, uint blockno);
void bwrite(struct buf *b);
void brelse(struct buf *b);
void bpin(struct buf *b);
void bunpin(struct buf *b);

#endif
