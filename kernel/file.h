#ifndef __FILE_H
#define __FILE_H

#include "types.h"
#include "fs.h"

struct pipe;

struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int ref;                 // 引用计数
    char readable;
    char writable;
    struct pipe *pipe;       // FD_PIPE 使用
    struct inode *ip;        // FD_INODE/FD_DEVICE 使用
    uint off;                // 当前偏移
    short major;             // 设备号
};

#define CONSOLE 1

void fileinit(void);
struct file *filealloc(void);
struct file *filedup(struct file *f);
void fileclose(struct file *f);
int filestat(struct file *f, uint64 addr);
int fileread(struct file *f, uint64 addr, int n);
int filewrite(struct file *f, uint64 addr, int n);

#endif
