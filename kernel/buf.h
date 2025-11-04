#ifndef __BUF_H
#define __BUF_H

#include "sleeplock.h"

// 块缓存层：在文件系统和磁盘驱动之间，缓存磁盘块减少I/O

#define BSIZE 4096  // 块大小4KB，比xv6的1KB大，提高吞吐

// 块缓冲区：缓存一个磁盘块，用LRU链表管理
struct buf {
    int valid;      // 数据是否有效(1=已读取，0=需要读)
    int disk;       // 是否正在I/O中
    uint dev;       // 设备号
    uint blockno;   // 块号
    struct sleeplock lock;  // 睡眠锁保护数据
    uint refcnt;    // 引用计数
    struct buf *prev;  // LRU双向链表
    struct buf *next;
    uchar data[BSIZE];  // 实际数据
};

// 操作接口
void binit(void);
struct buf *bread(uint dev, uint blockno);  // 读块
void bwrite(struct buf *b);                 // 同步写
void bsubmit_write(struct buf *b);          // 异步写(不等待)
void bwait(struct buf *b);                  // 等待I/O完成
void brelse(struct buf *b);                 // 释放
void bpin(struct buf *b);                   // 固定(防止替换)
void bunpin(struct buf *b);                 // 取消固定

#endif
