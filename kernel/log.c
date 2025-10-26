#include "types.h"
#include "param.h"
#include "buf.h"
#include "spinlock.h"
#include "fs.h"
#include "proc.h"
#include "global_func.h"

struct logheader {
    int n;
    int block[LOGSIZE];
};

struct log {
    struct spinlock lock;
    int start;
    int size;
    int outstanding; // 正在进行的文件系统操作数
    int committing;  // 是否正在提交
    int dev;
    struct logheader lh;
} log;

static void read_head(void)
{
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *lh = (struct logheader *)(buf->data);
    log.lh.n = lh->n;
    for (int i = 0; i < log.lh.n; i++)
        log.lh.block[i] = lh->block[i];
    brelse(buf);
}

static void write_head(void)
{
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *hb = (struct logheader *)(buf->data);
    hb->n = log.lh.n;
    for (int i = 0; i < log.lh.n; i++)
        hb->block[i] = log.lh.block[i];
    bwrite(buf);
    brelse(buf);
}

static void install_trans(void)
{
    for (int i = 0; i < log.lh.n; i++)
    {
        struct buf *lbuf = bread(log.dev, log.start + i + 1);
        struct buf *dbuf = bread(log.dev, log.lh.block[i]);
        memmove(dbuf->data, lbuf->data, BSIZE);
        bwrite(dbuf);
        brelse(lbuf);
        brelse(dbuf);
    }
}

static void write_log(void)
{
    for (int tail = 0; tail < log.lh.n; tail++)
    {
        struct buf *to = bread(log.dev, log.start + tail + 1);
        struct buf *from = bread(log.dev, log.lh.block[tail]);
        memmove(to->data, from->data, BSIZE);
        bwrite(to);
        brelse(from);
        brelse(to);
    }
}

void initlog(int dev, struct superblock *sb)
{
    if (sizeof(struct logheader) >= BSIZE)
        panic("initlog: header too big");

    initlock(&log.lock, "log");
    log.start = sb->logstart;
    log.size = sb->nlog;
    log.dev = dev;
    read_head();
    recover_from_log();
}

void begin_op(void)
{
    acquire(&log.lock);
    while (1)
    {
        if (log.committing)
        {
            sleep(&log, &log.lock);
        }
        else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE)
        {
            sleep(&log, &log.lock);
        }
        else
        {
            log.outstanding++;
            release(&log.lock);
            break;
        }
    }
}

void end_op(void)
{
    int do_commit = 0;

    acquire(&log.lock);
    log.outstanding--;
    if (log.committing)
        panic("log.committing");
    if (log.outstanding == 0)
    {
        do_commit = 1;
        log.committing = 1;
    }
    else
    {
        wakeup(&log);
    }
    release(&log.lock);

    if (!do_commit)
        return;

    if (log.lh.n > 0)
    {
        write_log();
        write_head();
        install_trans();
        log.lh.n = 0;
        write_head();
    }

    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
}

void recover_from_log(void)
{
    read_head();
    install_trans();
    log.lh.n = 0;
    write_head();
}

void log_write(struct buf *b)
{
    if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
        panic("too big a transaction");
    if (log.outstanding < 1)
        panic("log_write outside of trans");

    acquire(&log.lock);
    for (int i = 0; i < log.lh.n; i++)
    {
        if (log.lh.block[i] == b->blockno)
        {
            log.lh.block[i] = b->blockno;
            release(&log.lock);
            return;
        }
    }
    log.lh.block[log.lh.n] = b->blockno;
    log.lh.n++;
    bpin(b);
    release(&log.lock);
}
