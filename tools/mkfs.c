// 简易的文件系统打包工具，基于 xv6 的 mkfs 实现，并适配当前内核参数。
// 仅用于生成初始 fs.img，使内核能够挂载根目录。

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "tools/fs_format.h"

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

// 预留的 inode 数量，保持与 xv6 接近即可。
#define NINODES 200

static int nbitmap = FSSIZE / BPB + 1;
static int ninodeblocks = NINODES / IPB + 1;
static int nlog = LOGBLOCKS + 1; // 头块 + 日志数据块
static int nmeta;    // 元数据块数量
static int nblocks;  // 数据块数量

static int fsfd;
static struct superblock sb;
static char zeroes[BSIZE];
static uint freeinode = 1;
static uint freeblock;

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

static void wsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, SEEK_SET) != sec * BSIZE)
        die("lseek");
    if (write(fsfd, buf, BSIZE) != BSIZE)
        die("write");
}

static void rsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, SEEK_SET) != sec * BSIZE)
        die("lseek");
    if (read(fsfd, buf, BSIZE) != BSIZE)
        die("read");
}

static uint xint(uint x)
{
    uint y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

static ushort xshort(ushort x)
{
    ushort y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

static void winode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    struct dinode *dip = (struct dinode *)buf;
    dip += inum % IPB;
    *dip = *ip;
    wsect(bn, buf);
}

static void rinode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn = IBLOCK(inum, sb);
    rsect(bn, buf);
    struct dinode *dip = (struct dinode *)buf;
    dip += inum % IPB;
    *ip = *dip;
}

static void balloc(int used)
{
    // Mark blocks [0, used) as allocated in the bitmap.
    // This matches the kernel's expectation and xv6's mkfs behavior.
    char buf[BSIZE];
    memset(buf, 0, sizeof(buf));
    for (int b = 0; b < used; b++)
    {
        // Update the correct bitmap sector per block b
        rsect(BBLOCK(b, sb), buf);
        buf[(b % BPB) / 8] |= 1 << ((b % BPB) % 8);
        wsect(BBLOCK(b, sb), buf);
    }
}

static uint ialloc(ushort type)
{
    struct dinode din;

    memset(&din, 0, sizeof(din));
    din.type = xshort(type);
    din.nlink = xshort(1);
    din.size = xint(0);
    din.major = xshort(0);
    din.minor = xshort(0);

    freeinode++; // 分配新的 inode，下标从 1 开始
    winode(freeinode - 1, &din);
    return freeinode - 1;
}

static void iappend(uint inum, void *xp, int n)
{
    struct dinode din;
    char buf[BSIZE];
    uint off = 0;
    uint m;

    rinode(inum, &din);
    uint size = xint(din.size);
    uchar *p = (uchar *)xp;

    while (n > 0)
    {
        uint fbn = size / BSIZE;
        uint bn;
        if (fbn < NDIRECT)
        {
            if ((bn = xint(din.addrs[fbn])) == 0)
            {
                bn = freeblock++;
                din.addrs[fbn] = xint(bn);
            }
        }
        else
        {
            if (din.addrs[NDIRECT] == 0)
            {
                din.addrs[NDIRECT] = xint(freeblock++);
            }
            uint indirect[BSIZE / sizeof(uint)];
            rsect(xint(din.addrs[NDIRECT]), indirect);
            if ((bn = xint(indirect[fbn - NDIRECT])) == 0)
            {
                bn = freeblock++;
                indirect[fbn - NDIRECT] = xint(bn);
                wsect(xint(din.addrs[NDIRECT]), indirect);
            }
        }

        off = size % BSIZE;
        m = BSIZE - off;
        if (m > (uint)n)
            m = n;
        rsect(bn, buf);
        memmove(buf + off, p, m);
        wsect(bn, buf);
        size += m;
        p += m;
        n -= m;
    }

    din.size = xint(size);
    winode(inum, &din);
}

int main(int argc, char *argv[])
{
    static_assert(sizeof(int) == 4, "int must be 4 bytes");

    if (argc < 2)
    {
        fprintf(stderr, "Usage: mkfs fs.img [files...]\n");
        return 1;
    }

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0)
        die(argv[1]);

    nmeta = 2 + nlog + ninodeblocks + nbitmap;
    nblocks = FSSIZE - nmeta;

    sb.magic = xint(FSMAGIC);
    sb.size = xint(FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(NINODES);
    sb.nlog = xint(nlog);
    sb.logstart = xint(2);
    sb.inodestart = xint(2 + nlog);
    sb.bmapstart = xint(2 + nlog + ninodeblocks);

    fprintf(stdout, "mkfs: size=%u nblocks=%u ninodes=%u nlog=%u\n", FSSIZE, nblocks, NINODES, nlog);

    freeblock = nmeta;

    for (int i = 0; i < FSSIZE; i++)
        wsect(i, zeroes);

    char buf[BSIZE];
    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    uint rootino = ialloc(T_DIR);
    assert(rootino == ROOTINO);

    struct dirent de;
    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));

    for (int i = 2; i < argc; i++)
    {
        char *shortname = argv[i];
        if (strncmp(shortname, "user/", 5) == 0)
            shortname += 5;
        if (shortname[0] == '_')
            shortname += 1;

        if (strlen(shortname) > DIRSIZ)
        {
            fprintf(stderr, "mkfs: filename %s too long\n", shortname);
            exit(1);
        }

        int fd = open(argv[i], O_RDONLY);
        if (fd < 0)
            die(argv[i]);

        uint inum = ialloc(T_FILE);
        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, shortname, DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        while (1)
        {
            int cc = read(fd, buf, sizeof(buf));
            if (cc < 0)
                die("read");
            if (cc == 0)
                break;
            iappend(inum, buf, cc);
        }
        close(fd);
    }

    // Mark all blocks up to the current freeblock as allocated in the bitmap,
    // including metadata and the data blocks we've written so far.
    balloc(freeblock);
    close(fsfd);
    return 0;
}
