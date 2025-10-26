#ifndef __PIPE_H
#define __PIPE_H

#include "types.h"
#include "spinlock.h"
#include "buf.h"

struct pipe {
    struct spinlock lock;
    char data[BSIZE];
    uint nread;     // 读取位置
    uint nwrite;    // 写入位置
    int readopen;   // 读取端是否打开
    int writeopen;  // 写入端是否打开
};

int pipealloc(struct file **f0, struct file **f1);
void pipeclose(struct pipe *p, int writable);
int pipewrite(struct pipe *p, uint64 addr, int n);
int piperead(struct pipe *p, uint64 addr, int n);

#endif
