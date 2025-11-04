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

// read_head: 从磁盘读日志头到内存
static void read_head(void)
{
    struct buf *buf = bread(logstate.dev, logstate.start);
    struct logheader *lh = (struct logheader *)(buf->data);
    logstate.lh.n = lh->n;
    for (int i = 0; i < logstate.lh.n; i++)
        logstate.lh.block[i] = lh->block[i];
    brelse(buf);
}

// write_head: 写日志头到磁盘，这是事务的提交点(原子操作)
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

// install_trans: 将日志块安装到实际位置，批量异步写(窗口=8)
static void install_trans(int recovering)
{
    const int WINDOW = 8;
    int n = logstate.lh.n;
    
    for (int base = 0; base < n; base += WINDOW)
    {
        int cnt = (base + WINDOW <= n) ? WINDOW : (n - base);
        struct buf *held[WINDOW];
        
        // 批量提交写请求
        for (int i = 0; i < cnt; i++)
        {
            int idx = base + i;
            struct buf *lbuf = bread(logstate.dev, logstate.start + idx + 1);
            struct buf *dbuf = bread(logstate.dev, logstate.lh.block[idx]);
            memmove(dbuf->data, lbuf->data, BSIZE);
            bsubmit_write(dbuf);
            if (!recovering)
                bunpin(dbuf);
            brelse(lbuf);
            held[i] = dbuf;
        }
        
        // 统一等待完成
        for (int i = 0; i < cnt; i++)
        {
            bwait(held[i]);
            brelse(held[i]);
        }
    }
}

// write_log: 将修改的块写入日志区，批量异步写
static void write_log(void)
{
    const int WINDOW = 8;
    int n = logstate.lh.n;
    
    for (int base = 0; base < n; base += WINDOW)
    {
        int cnt = (base + WINDOW <= n) ? WINDOW : (n - base);
        struct buf *held[WINDOW];
        
        for (int i = 0; i < cnt; i++)
        {
            int tail = base + i;
            struct buf *to = bread(logstate.dev, logstate.start + tail + 1);
            struct buf *from = bread(logstate.dev, logstate.lh.block[tail]);
            memmove(to->data, from->data, BSIZE);
            bsubmit_write(to);
            brelse(from);
            held[i] = to;
        }
        
        for (int i = 0; i < cnt; i++)
        {
            bwait(held[i]);
            brelse(held[i]);
        }
    }
}

// log_init: 初始化日志系统，启动时调用
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

// begin_transaction: 开始文件系统事务，等待日志空间和提交完成
void begin_transaction(void)
{
    acquire(&logstate.lock);
    while (1)
    {
        if (logstate.committing)  // 正在提交，等待
            sleep(&logstate, &logstate.lock);
        else if (logstate.lh.n + (logstate.outstanding + 1) * MAXOPBLOCKS > LOGSIZE)
            sleep(&logstate, &logstate.lock);  // 日志可能满，等待
        else
        {
            logstate.outstanding++;
            release(&logstate.lock);
            break;
        }
    }
}

// end_transaction: 结束事务，最后一个操作时提交日志
// 提交流程：write_log(批量) -> write_head(提交点) -> install_trans(批量) -> 清日志
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
        write_log();
        write_head();  // 提交点
        
        if (force_crash_after_header) {
            printf("log: commit header written, n=%d firstblk=%d\n", logstate.lh.n,
                   logstate.lh.n > 0 ? logstate.lh.block[0] : -1);
            panic("log: forced crash after header");
        }
        
        install_trans(0);
        logstate.lh.n = 0;
        write_head();  // 清日志
    }

    acquire(&logstate.lock);
    logstate.committing = 0;
    wakeup(&logstate);
    release(&logstate.lock);
}

// recover_log: 启动时恢复未完成的事务
void recover_log(void)
{
    read_head();
    if (logstate.lh.n > 0) {
        printf("recover: found n=%d firstblk=%d\n", logstate.lh.n,
               logstate.lh.block[0]);
    }
    install_trans(1);  // 重放日志
    logstate.lh.n = 0;
    write_head();
    if (logstate.lh.n == 0)
        printf("recover: done\n");
}

// log_block_write: 记录要写的块，提交时会写入日志
// 日志吸收：同一块多次修改只记录一次
void log_block_write(struct buf *b)
{
    if (logstate.lh.n >= LOGSIZE || logstate.lh.n >= logstate.size - 1)
        panic("too big a transaction");
    if (logstate.outstanding < 1)
        panic("log_write outside of trans");

    acquire(&logstate.lock);
    
    // 日志吸收：检查是否已在日志中
    for (int i = 0; i < logstate.lh.n; i++)
    {
        if (logstate.lh.block[i] == b->blockno)
        {
            logstate.lh.block[i] = b->blockno;
            release(&logstate.lock);
            return;
        }
    }
    
    // 新块，加入日志并pin住(防止被LRU替换)
    logstate.lh.block[logstate.lh.n] = b->blockno;
    logstate.lh.n++;
    bpin(b);
    release(&logstate.lock);
}
