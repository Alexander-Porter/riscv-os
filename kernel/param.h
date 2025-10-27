#ifndef __PARAM_H
#define __PARAM_H

// 内核全局常量定义，参考 xv6 并按当前实验场景裁剪
#define NPROC        64    // 最多进程数
#define NCPU         1     // 当前实验仅使用单核
#define NOFILE       16    // 每个进程最多打开的文件数
#define NFILE        100   // 系统范围内的文件表项数
#define NINODE       50    // 内存中缓存的 inode 数
#define NDEV         10    // 设备数量上限
#define NBUF         30    // 块缓存数量，足够支撑日志系统

#define MAXARG       16   // exec 最多支持的参数个数
#define MAXARGLEN    128  // 单个参数的最大长度
#define PROG_NAME_MAX 32  // 用户程序名称最大长度
#define USERSTACK_PAGES 1 // 用户栈使用的页数量

#define ROOTDEV      1     // 根文件系统所在设备号
#define MAXPATH      128   // 路径最大长度
#define MAXOPBLOCKS  10    // 单次文件系统操作占用的最大日志块数
#define LOGBLOCKS    (MAXOPBLOCKS * 3)
#define LOGSIZE      LOGBLOCKS
#define FSSIZE       200000  // 文件系统总块数（仅用于 mkfs）

// 懒分配 sbrk
#ifndef ENABLE_LAZY_SBRK
#define ENABLE_LAZY_SBRK 1
#endif

#endif
