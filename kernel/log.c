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

// 手册命名：日志系统状态
struct log_state {
    struct spinlock lock;
    int start;
    int size;
    int outstanding; // 正在进行的文件系统操作数
    int committing;  // 是否正在提交
    int dev;
    struct logheader lh;
} logstate;

// 调试开关：在提交时写完日志头后立刻崩溃，用于模拟“日志已持久化但数据未安装”的电源故障
static volatile int force_crash_after_header = 0;

void log_set_crash_mode(int enable)
{
    force_crash_after_header = enable ? 1 : 0;
}

static void read_head(void)
{
    struct buf *buf = bread(logstate.dev, logstate.start);
    struct logheader *lh = (struct logheader *)(buf->data);
    logstate.lh.n = lh->n;
    for (int i = 0; i < logstate.lh.n; i++)
        logstate.lh.block[i] = lh->block[i];
    brelse(buf);
}

static void write_head(void)
{
    struct buf *buf = bread(logstate.dev, logstate.start);
    struct logheader *hb = (struct logheader *)(buf->data);
    hb->n = logstate.lh.n;
    for (int i = 0; i < logstate.lh.n; i++)
        hb->block[i] = logstate.lh.block[i];
    bwrite(buf);
    brelse(buf);
}

static void install_trans(int recovering)
{
    for (int i = 0; i < logstate.lh.n; i++)
    {
        struct buf *lbuf = bread(logstate.dev, logstate.start + i + 1);
        struct buf *dbuf = bread(logstate.dev, logstate.lh.block[i]);
        memmove(dbuf->data, lbuf->data, BSIZE);
        bwrite(dbuf);
        // 在正常提交路径中，解除对目标块的 pin，以避免耗尽缓冲区
        if (!recovering)
            bunpin(dbuf);
        brelse(lbuf);
        brelse(dbuf);
    }
}

static void write_log(void)
{
    for (int tail = 0; tail < logstate.lh.n; tail++)
    {
        struct buf *to = bread(logstate.dev, logstate.start + tail + 1);
        struct buf *from = bread(logstate.dev, logstate.lh.block[tail]);
        memmove(to->data, from->data, BSIZE);
        bwrite(to);
        brelse(from);
        brelse(to);
    }
}

void log_init(int dev, struct superblock *sb)
{
    if (sizeof(struct logheader) >= BSIZE)
        panic("initlog: header too big");

    initlock(&logstate.lock, "log");
    logstate.start = sb->logstart;
    logstate.size = sb->nlog;
    logstate.dev = dev;
    read_head();
    recover_log();
}

void begin_transaction(void)
{
    acquire(&logstate.lock);
    while (1)
    {
        if (logstate.committing)
        {
            sleep(&logstate, &logstate.lock);
        }
        else if (logstate.lh.n + (logstate.outstanding + 1) * MAXOPBLOCKS > LOGSIZE)
        {
            sleep(&logstate, &logstate.lock);
        }
        else
        {
            logstate.outstanding++;
            release(&logstate.lock);
            break;
        }
    }
}

void end_transaction(void)
{
    int do_commit = 0;

    acquire(&logstate.lock);
    logstate.outstanding--;
    if (logstate.committing)
        panic("log.committing");
    if (logstate.outstanding == 0)
    {
        do_commit = 1;
        logstate.committing = 1;
    }
    else
    {
        wakeup(&logstate);
    }
    release(&logstate.lock);

    if (!do_commit)
        return;

    if (logstate.lh.n > 0)
    {
        write_log();              // 1) 将数据块写入日志区域
        write_head();             // 2) 写入日志头(真正的提交点)
        if (force_crash_after_header) {
            printf("log: commit header written, n=%d firstblk=%d\n", logstate.lh.n,
                   logstate.lh.n > 0 ? logstate.lh.block[0] : -1);
            panic("log: forced crash after header"); // 3) 模拟掉电：日志存在但未安装
        }
        install_trans(0);        // 4) 安装到 home 位置
        logstate.lh.n = 0;
        write_head();            // 5) 清空日志
    }

    acquire(&logstate.lock);
    logstate.committing = 0;
    wakeup(&logstate);
    release(&logstate.lock);
}

void recover_log(void)
{
    read_head();
    if (logstate.lh.n > 0) {
        printf("recover: found n=%d firstblk=%d\n", logstate.lh.n,
               logstate.lh.block[0]);
    }
    install_trans(1);
    logstate.lh.n = 0;
    write_head();
    if (logstate.lh.n == 0)
        printf("recover: done\n");
}

void log_block_write(struct buf *b)
{
    if (logstate.lh.n >= LOGSIZE || logstate.lh.n >= logstate.size - 1)
        panic("too big a transaction");
    if (logstate.outstanding < 1)
        panic("log_write outside of trans");

    acquire(&logstate.lock);
    for (int i = 0; i < logstate.lh.n; i++)
    {
        if (logstate.lh.block[i] == b->blockno)
        {
            logstate.lh.block[i] = b->blockno;
            release(&logstate.lock);
            return;
        }
    }
    logstate.lh.block[logstate.lh.n] = b->blockno;
    logstate.lh.n++;
    bpin(b);
    release(&logstate.lock);
}
