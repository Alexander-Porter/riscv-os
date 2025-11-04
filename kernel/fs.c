// 文件系统核心：inode管理、块分配、目录和路径解析

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "buf.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "global_func.h"
#include "fs.h"
#include "file.h"

static inline uint min(uint a, uint b)
{
    return a < b ? a : b;
}

struct superblock sb;

// readsb: 从磁盘块1读取超级块
void readsb(int dev, struct superblock *sbp)
{
    struct buf *b = bread(dev, 1);
    memmove(sbp, b->data, sizeof(*sbp));
    brelse(b);
}

// inode缓存：50个inode槽，自旋锁保护表，睡眠锁保护单个inode
struct {
    struct spinlock lock;
    struct inode inode[NINODE];
} icache;

// iinit: 初始化inode缓存，启动时调用
void iinit(void)
{
    initlock(&icache.lock, "icache");
    for (int i = 0; i < NINODE; i++)
    {
        initsleeplock(&icache.inode[i].lock, "inode");
    }
}

// iget: 获取inode缓存项，先查缓存再分配空槽，返回未锁定的inode
static struct inode *iget(uint dev, uint inum)
{
    struct inode *empty = 0;

    acquire(&icache.lock);
    
    // 先在缓存中找
    for (struct inode *ip = icache.inode; ip < icache.inode + NINODE; ip++)
    {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum)
        {
            ip->ref++;
            release(&icache.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0)
            empty = ip;
    }

    // 分配新槽
    if (empty == 0)
        panic("iget: no inodes");

    struct inode *ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;  // 数据未加载，ilock时读盘
    release(&icache.lock);
    return ip;
}

// ialloc: 分配新inode，扫描磁盘找type=0的，设置类型后返回
struct inode *ialloc(uint dev, short type)
{
    for (uint inum = 1; inum < sb.ninodes; inum++)
    {
        struct buf *bp = bread(dev, IBLOCK(inum, sb));
        struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
        if (dip->type == 0)
        {
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            log_block_write(bp);
            brelse(bp);
            return iget(dev, inum);
        }
        brelse(bp);
    }
    panic("ialloc: no inodes");
    return 0;
}

// idup: 增加inode引用计数
struct inode *idup(struct inode *ip)
{
    acquire(&icache.lock);
    ip->ref++;
    release(&icache.lock);
    return ip;
}

// ilock: 锁定inode并从磁盘加载数据(若valid=0)
void ilock(struct inode *ip)
{
    if (ip == 0 || ip->ref < 1)
        panic("ilock");

    acquiresleep(&ip->lock);

    if (ip->valid == 0)
    {
        struct buf *bp = bread(ip->dev, IBLOCK(ip->inum, sb));
        struct dinode *dip = (struct dinode *)bp->data + ip->inum % IPB;
        
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        
        brelse(bp);
        ip->valid = 1;
        if (ip->type == 0)
            panic("ilock: no type");
    }
}

// iunlock: 解锁inode
void iunlock(struct inode *ip)
{
    if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");

    releasesleep(&ip->lock);
}

// iput: 减少引用计数，ref=0且nlink=0时删除inode
void iput(struct inode *ip)
{
    acquire(&icache.lock);
    
    if (ip->ref == 1 && ip->valid && ip->nlink == 0)
    {
        release(&icache.lock);
        
        ilock(ip);
        itrunc(ip);   // 释放所有块
        ip->type = 0;
        iupdate(ip);
        releasesleep(&ip->lock);

        acquire(&icache.lock);
        ip->valid = 0;
    }
    
    ip->ref--;
    release(&icache.lock);
}

// iunlockput: 解锁+释放引用
void iunlockput(struct inode *ip)
{
    iunlock(ip);
    iput(ip);
}

// iupdate: 将内存inode写回磁盘
void iupdate(struct inode *ip)
{
    struct buf *bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    struct dinode *dip = (struct dinode *)bp->data + ip->inum % IPB;
    
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    
    log_block_write(bp);
    brelse(bp);
}

// 块分配优化：记录上次找到的空闲块位置
static uint next_free_b_hint = 0;

// balloc: 分配数据块，用位图扫描+hint优化，两趟搜索
static uint balloc(uint dev)
{
    uint start = next_free_b_hint;
    
    // 从hint开始扫描
    for (int pass = 0; pass < 2; pass++)
    {
        for (uint b = start - (start % BPB); b < sb.size; b += BPB)
        {
            struct buf *bp = bread(dev, BBLOCK(b, sb));
            int bi_start = (b == start - (start % BPB)) ? (start % BPB) : 0;
            
            for (int bi = bi_start; bi < BPB && b + bi < sb.size; bi++)
            {
                int m = 1 << (bi % 8);
                if ((bp->data[bi / 8] & m) == 0)
                {
                    bp->data[bi / 8] |= m;
                    log_block_write(bp);
                    brelse(bp);
                    
                    uint found = b + bi;
                    struct buf *bb = bread(dev, found);
                    memset(bb->data, 0, BSIZE);
                    log_block_write(bb);
                    brelse(bb);
                    
                    next_free_b_hint = found + 1;
                    if (next_free_b_hint >= sb.size)
                        next_free_b_hint = 0;
                    
                    return found;
                }
            }
            brelse(bp);
        }
        
        // 第二趟：从0到hint
        start = 0;
        if (next_free_b_hint == 0)
            break;
        
        for (uint b = 0; b < next_free_b_hint; b += BPB)
        {
            struct buf *bp = bread(dev, BBLOCK(b, sb));
            for (int bi = 0; bi < BPB && b + bi < next_free_b_hint; bi++)
            {
                int m = 1 << (bi % 8);
                if ((bp->data[bi / 8] & m) == 0)
                {
                    bp->data[bi / 8] |= m;
                    log_block_write(bp);
                    brelse(bp);
                    
                    uint found = b + bi;
                    struct buf *bb = bread(dev, found);
                    memset(bb->data, 0, BSIZE);
                    log_block_write(bb);
                    brelse(bb);
                    
                    next_free_b_hint = found + 1;
                    if (next_free_b_hint >= sb.size)
                        next_free_b_hint = 0;
                    
                    return found;
                }
            }
            brelse(bp);
        }
        break;
    }
    
    panic("balloc: out of blocks");
    return 0;
}

// bfree: 释放数据块，位图清0
static void bfree(int dev, uint b)
{
    struct buf *bp = bread(dev, BBLOCK(b, sb));
    int bi = b % BPB;
    int m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0)
        panic("freeing free block");
    bp->data[bi / 8] &= ~m;
    log_block_write(bp);
    brelse(bp);
}

// bmap: 映射文件内偏移到块号，按需分配直接块和间接块
static uint bmap(struct inode *ip, uint bn)
{
    if (bn < NDIRECT)
    {
        if (ip->addrs[bn] == 0)
            ip->addrs[bn] = balloc(ip->dev);
        return ip->addrs[bn];
    }
    bn -= NDIRECT;

    if (bn < NINDIRECT)
    {
        if (ip->addrs[NDIRECT] == 0)
            ip->addrs[NDIRECT] = balloc(ip->dev);
        struct buf *bp = bread(ip->dev, ip->addrs[NDIRECT]);
        uint *a = (uint *)bp->data;
        if (a[bn] == 0)
        {
            a[bn] = balloc(ip->dev);
            log_block_write(bp);
        }
        uint addr = a[bn];
        brelse(bp);
        return addr;
    }
    panic("bmap: out of range");
    return 0;
}

// itrunc: 释放inode所有数据块(直接块+间接块)
void itrunc(struct inode *ip)
{
    for (int i = 0; i < NDIRECT; i++)
    {
        if (ip->addrs[i])
        {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT])
    {
        struct buf *bp = bread(ip->dev, ip->addrs[NDIRECT]);
        uint *a = (uint *)bp->data;
        for (int j = 0; j < NINDIRECT; j++)
        {
            if (a[j])
                bfree(ip->dev, a[j]);
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }
    ip->size = 0;
    iupdate(ip);
}

// readi: 从inode读数据到dst，逐块复制
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
    if (off > ip->size || off + n < off)
        return -1;

    if (off + n > ip->size)
        n = ip->size - off;

    uint tot = 0;
    while (tot < n)
    {
        uint bn = bmap(ip, off / BSIZE);
        struct buf *bp = bread(ip->dev, bn);
        uint m = min(n - tot, BSIZE - off % BSIZE);
        if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1)
        {
            brelse(bp);
            break;
        }
        brelse(bp);
        tot += m;
        off += m;
        dst += m;
    }
    return tot;
}

// writei: 向inode写数据，按需扩展文件
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE * BSIZE)
        return -1;

    uint tot = 0;
    while (tot < n)
    {
        uint bn = bmap(ip, off / BSIZE);
        struct buf *bp = bread(ip->dev, bn);
        uint m = min(n - tot, BSIZE - off % BSIZE);
        if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1)
        {
            brelse(bp);
            break;
        }
    log_block_write(bp);
        brelse(bp);
        tot += m;
        off += m;
        src += m;
    }

    if (n > 0)
    {
        if (off > ip->size)
            ip->size = off;
        iupdate(ip);
    }
    return tot;
}

// stati: 填充stat结构
void stati(struct inode *ip, struct stat *st)
{
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->nlink;
    st->size = ip->size;
}

// count_free_blocks: 调试用，扫描位图统计空闲块数
int count_free_blocks(void)
{
    int free = 0;
    for (uint b = 0; b < sb.size; b += BPB)
    {
        struct buf *bp = bread(ROOTDEV, BBLOCK(b, sb));
        for (int bi = 0; bi < BPB && b + bi < sb.size; bi++)
        {
            int m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0)
                free++;
        }
        brelse(bp);
    }
    return free;
}

// count_free_inodes: 调试用，遍历inode统计type=0的数量
int count_free_inodes(void)
{
    int free = 0;
    for (uint inum = 1; inum < sb.ninodes; inum++)
    {
        struct buf *bp = bread(ROOTDEV, IBLOCK(inum, sb));
        struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
        if (dip->type == 0)
            free++;
        brelse(bp);
    }
    return free;
}

extern int buffer_cache_hits, buffer_cache_misses, disk_read_count, disk_write_count;

// debug_filesystem_state: 打印文件系统状态和统计信息
void debug_filesystem_state(void)
{
    printf("=== Filesystem Debug Info ===\n");
    printf("Total blocks: %d\n", sb.size);
    printf("Data blocks: %d\n", sb.nblocks);
    printf("Inodes: %d\n", sb.ninodes);
    printf("Log blocks: %d (start=%d)\n", sb.nlog, sb.logstart);
    printf("Inode start: %d  Bmap start: %d\n", sb.inodestart, sb.bmapstart);
    printf("Free blocks: %d\n", count_free_blocks());
    printf("Free inodes: %d\n", count_free_inodes());
    printf("Buf cache hits: %d  misses: %d\n", buffer_cache_hits, buffer_cache_misses);
    printf("Disk reads: %d  writes: %d\n", disk_read_count, disk_write_count);
}

// debug_inode_usage: 打印缓存中活跃的inode
void debug_inode_usage(void)
{
    printf("=== Inode Usage ===\n");
    acquire(&icache.lock);
    for (int i = 0; i < NINODE; i++)
    {
        struct inode *ip = &icache.inode[i];
        if (ip->ref > 0)
        {
            printf("Inode %d: ref=%d, type=%d, size=%d\n", ip->inum, ip->ref, ip->type, ip->size);
        }
    }
    release(&icache.lock);
}

// namecmp: 目录项名字比较
int namecmp(const char *s, const char *t)
{
    return strncmp(s, t, DIRSIZ);
}

// dir_lookup: 在目录中查找name，返回inode
struct inode *dir_lookup(struct inode *dp, char *name, uint *poff)
{
    if (dp->type != T_DIR)
        panic("dirlookup not DIR");

    for (uint off = 0; off < dp->size; off += sizeof(struct dirent))
    {
        struct dirent de;
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        if (de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0)
        {
            if (poff)
                *poff = off;
            return iget(dp->dev, de.inum);
        }
    }

    return 0;
}

// dir_link: 在目录中添加新的目录项
int dir_link(struct inode *dp, char *name, uint inum)
{
    struct dirent de;
    uint off;

    if ((dir_lookup(dp, name, 0)) != 0)
        return -1;

    for (off = 0; off < dp->size; off += sizeof(de))
    {
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        if (de.inum == 0)
            break;
    }

    memset(de.name, 0, DIRSIZ);
    safestrcpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        panic("dirlink");

    return 0;
}

// skipelem: 解析路径，提取一个路径分量到name
static char *skipelem(char *path, char *name)
{
    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    char *s = path;
    while (*path != '/' && *path != 0)
        path++;
    int len = path - s;
    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else
    {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

// namex: 路径解析核心，nameiparent=1返回父目录
static struct inode *namex(char *path, int nameiparent, char *name)
{
    struct inode *ip;
    if (*path == '/')
        ip = iget(ROOTDEV, ROOTINO);
    else
        ip = idup(myproc()->cwd);

    while ((path = skipelem(path, name)) != 0)
    {
        ilock(ip);
        if (ip->type != T_DIR)
        {
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0')
        {
            iunlock(ip);
            return ip;
        }
        struct inode *next = dir_lookup(ip, name, 0);
        if (next == 0)
        {
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if (nameiparent)
    {
        iput(ip);
        return 0;
    }
    return ip;
}

// path_walk: 解析完整路径，返回最终inode
struct inode *path_walk(char *path)
{
    char name[DIRSIZ];
    return namex(path, 0, name);
}

// path_parent: 返回父目录inode，name存最后一个分量
struct inode *path_parent(char *path, char *name)
{
    return namex(path, 1, name);
}
