#ifndef __FS_H
#define __FS_H

#include "types.h"
#include "param.h"
#include "buf.h"
#include "sleeplock.h"
#include "stat.h"

// 文件系统层：在块缓存和文件描述符之间，管理inode、目录、路径解析

#define ROOTINO 1  // 根目录的inode编号，inode 0不用

// 磁盘布局：boot | super | log | inode blocks | bitmap | data blocks
struct superblock {
    uint magic;      // 魔数0x10203040，验证文件系统
    uint size;       // 文件系统总块数
    uint nblocks;    // 数据块数量
    uint ninodes;    // inode总数
    uint nlog;       // 日志块数量
    uint logstart;   // 日志区起始块号
    uint inodestart; // inode区起始块号
    uint bmapstart;  // 位图区起始块号
};

#define FSMAGIC 0x10203040

// 块寻址：12个直接块 + 1个间接块(可指向1024个块) = 最大1036块 ≈ 4MB
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))  // 4096/4=1024
#define MAXFILE (NDIRECT + NINDIRECT)

// 磁盘inode：存储在磁盘上的inode结构
struct dinode {
    short type;    // 文件类型：T_FILE/T_DIR/T_DEVICE/0(空闲)
    short major;   // 主设备号
    short minor;   // 次设备号
    short nlink;   // 硬链接数，为0时可删除
    uint size;     // 文件大小(字节)
    uint addrs[NDIRECT + 1];  // 前12个直接块，第13个间接块
};

#define IPB (BSIZE / sizeof(struct dinode))  // 每块的inode数
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)  // inode所在块号

// 位图：管理数据块分配状态
#define BPB (BSIZE * 8)  // 每个位图块管理的块数
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)  // 块b的位图所在块号

// 目录项：目录文件内容就是dirent数组
#define DIRSIZ 14

struct dirent {
    ushort inum;        // inode编号，0表示空闲
    char name[DIRSIZ];  // 文件名，可能没有\0
};

extern struct superblock sb;

#ifdef __cplusplus
extern "C" {
#endif

// 初始化
void readsb(int dev, struct superblock *sb);
void iinit(void);

// inode操作
struct inode *ialloc(uint dev, short type);  // 分配新inode
struct inode *idup(struct inode *ip);        // 增加引用计数
void ilock(struct inode *ip);                // 锁定并从磁盘加载
void iunlock(struct inode *ip);              // 解锁
void iunlockput(struct inode *ip);           // 解锁+释放
void iput(struct inode *ip);                 // 减少引用(可能删除)
void iupdate(struct inode *ip);              // 写回磁盘
void itrunc(struct inode *ip);               // 清空文件，释放所有块
void stati(struct inode *ip, struct stat *st);

// 文件读写
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);

// 目录操作
struct inode *dir_lookup(struct inode *dp, char *name, uint *poff);
int dir_link(struct inode *dp, char *name, uint inum);

// 路径解析
struct inode *path_walk(char *path);
struct inode *path_parent(char *path, char *name);

// 日志系统(崩溃一致性)
void begin_transaction(void);
void end_transaction(void);
void log_block_write(struct buf *b);
void recover_log(void);
void log_init(int dev, struct superblock *sb);
void log_set_crash_mode(int enable);  // 调试：模拟崩溃

// 调试接口
void debug_filesystem_state(void);
void debug_inode_usage(void);

#ifdef __cplusplus
}
#endif

// 内存inode：缓存磁盘inode，避免频繁读盘
// ref>0表示有进程使用，valid=1表示数据已加载，持有lock才能访问内容
struct inode {
    uint dev;
    uint inum;
    int ref;                // 引用计数
    struct sleeplock lock;
    int valid;              // 数据是否已从磁盘加载

    // 下面的字段从磁盘dinode拷贝(仅在valid=1时有效)
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

#endif
