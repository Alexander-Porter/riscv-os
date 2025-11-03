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

void readsb(int dev, struct superblock *sbp)
{
    struct buf *b = bread(dev, 1);
    memmove(sbp, b->data, sizeof(*sbp));
    brelse(b);
}

// 内存 inode 缓存
struct {
    struct spinlock lock;
    struct inode inode[NINODE];
} icache;

void iinit(void)
{
    initlock(&icache.lock, "icache");
    for (int i = 0; i < NINODE; i++)
    {
        initsleeplock(&icache.inode[i].lock, "inode");
    }
}

static struct inode *iget(uint dev, uint inum)
{
    struct inode *empty = 0;

    acquire(&icache.lock);
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

    if (empty == 0)
        panic("iget: no inodes");

    struct inode *ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    release(&icache.lock);
    return ip;
}

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
    return 0; // unreachable
}

struct inode *idup(struct inode *ip)
{
    acquire(&icache.lock);
    ip->ref++;
    release(&icache.lock);
    return ip;
}

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

void iunlock(struct inode *ip)
{
    if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");

    releasesleep(&ip->lock);
}

void iput(struct inode *ip)
{
    acquire(&icache.lock);
    if (ip->ref == 1 && ip->valid && ip->nlink == 0)
    {
        release(&icache.lock);
        ilock(ip);
        itrunc(ip);
        ip->type = 0;
        iupdate(ip);
        releasesleep(&ip->lock);

        acquire(&icache.lock);
        ip->valid = 0;
    }
    ip->ref--;
    release(&icache.lock);
}

void iunlockput(struct inode *ip)
{
    iunlock(ip);
    iput(ip);
}

// 将内存 inode 回写到磁盘
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

// 下一个空闲块扫描的提示指针，避免每次都从0开始全盘扫描
static uint next_free_b_hint = 0;

static uint balloc(uint dev)
{
    // 从提示位置开始扫描一次，必要时回绕到0继续扫描到提示位置
    uint start = next_free_b_hint;
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
        // 回绕，再扫描一次直到原始起点
        start = 0;
        if (next_free_b_hint == 0)
            break;
        // 限定第二趟扫描的终点为原起点之前
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
    return 0; // unreachable
}

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
    return 0; // unreachable
}

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

void stati(struct inode *ip, struct stat *st)
{
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->nlink;
    st->size = ip->size;
}

// ====================== 调试与统计辅助 ======================
// 统计空闲块数量：扫描位图，统计为0的比特数
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

// 统计空闲inode数量：遍历所有inode磁盘块，type==0视为空闲
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

int namecmp(const char *s, const char *t)
{
    return strncmp(s, t, DIRSIZ);
}

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

struct inode *path_walk(char *path)
{
    char name[DIRSIZ];
    return namex(path, 0, name);
}

struct inode *path_parent(char *path, char *name)
{
    return namex(path, 1, name);
}
