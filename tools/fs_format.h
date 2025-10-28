#ifndef FS_FORMAT_H
#define FS_FORMAT_H

#include "kernel/types.h"

// 磁盘布局定义，供用户态 mkfs 工具使用（参考 xv6-riscv）。
#define ROOTINO  1
#define BSIZE    4096  // 修改为4KB以支持更大文件

struct superblock {
    uint magic;
    uint size;
    uint nblocks;
    uint ninodes;
    uint nlog;
    uint logstart;
    uint inodestart;
    uint bmapstart;
};

#define FSMAGIC 0x10203040

#define NDIRECT   12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE   (NDIRECT + NINDIRECT)

struct dinode {
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

#define IPB    (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb) ((i) / IPB + (sb).inodestart)

#define BPB    (BSIZE * 8)
#define BBLOCK(b, sb) ((b) / BPB + (sb).bmapstart)

#define DIRSIZ 14

struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

#endif
