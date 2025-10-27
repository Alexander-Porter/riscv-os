#ifndef __FS_H
#define __FS_H

#include "types.h"
#include "param.h"
#include "buf.h"
#include "sleeplock.h"
#include "stat.h"

#define ROOTINO 1       // 根目录 inode 编号

// 磁盘布局：boot | super | log | inode blocks | bitmap | data blocks
struct superblock {
    uint magic;      // 魔数
    uint size;       // 文件系统总块数
    uint nblocks;    // 数据块数量
    uint ninodes;    // inode 数量
    uint nlog;       // 日志块数量
    uint logstart;   // 日志起始块号
    uint inodestart; // inode 区域起始块号
    uint bmapstart;  // 位图区域起始块号
};

#define FSMAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

struct dinode {
    short type;          // 文件类型
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

// 每块包含的 inode 数
#define IPB (BSIZE / sizeof(struct dinode))

// 给定 inode 编号所在的块号
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// 位图相关宏
#define BPB (BSIZE * 8)
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

#define DIRSIZ 14

struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

extern struct superblock sb;

#ifdef __cplusplus
extern "C" {
#endif

void readsb(int dev, struct superblock *sb);
void iinit(void);
struct inode *ialloc(uint dev, short type);
struct inode *idup(struct inode *ip);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iunlockput(struct inode *ip);
void iput(struct inode *ip);
void iupdate(struct inode *ip);
void itrunc(struct inode *ip);
void stati(struct inode *ip, struct stat *st);
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);
struct inode *dir_lookup(struct inode *dp, char *name, uint *poff);
int dir_link(struct inode *dp, char *name, uint inum);
struct inode *path_walk(char *path);
struct inode *path_parent(char *path, char *name);

void begin_transaction(void);
void end_transaction(void);
void log_block_write(struct buf *b);
void recover_log(void);
void log_init(int dev, struct superblock *sb);
// 调试：控制提交时的强制崩溃，用于模拟日志-数据未安装的场景
void log_set_crash_mode(int enable);

// 调试辅助接口（由 fs.c 提供）
void debug_filesystem_state(void);
void debug_inode_usage(void);

#ifdef __cplusplus
}
#endif

struct inode {
    uint dev;
    uint inum;
    int ref;
    struct sleeplock lock;
    int valid;

    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

#endif
