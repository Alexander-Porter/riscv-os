#ifndef __STAT_H
#define __STAT_H

#include "types.h"

#define T_DIR  1   // 目录
#define T_FILE 2   // 普通文件
#define T_DEVICE 3 // 字符设备
#define T_SYMLINK 4 // 符号链接

struct stat {
    uint dev;
    uint ino;
    short type;
    short nlink;
    uint64 size;
};

#endif
