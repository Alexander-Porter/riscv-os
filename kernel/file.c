#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "pipe.h"
#include "stat.h"
#include "global_func.h"

// 全局文件表，所有打开的文件都在这里
struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

// 初始化文件表
void fileinit(void)
{
    initlock(&ftable.lock, "ftable");
}

// 分配一个空闲的文件结构
struct file *filealloc(void)
{
    acquire(&ftable.lock);
    for (struct file *f = ftable.file; f < ftable.file + NFILE; f++)
    {
        if (f->ref == 0)  // 找到未使用的文件项
        {
            f->ref = 1;
            f->type = FD_NONE;
            f->readable = 0;
            f->writable = 0;
            f->off = 0;
            f->ip = 0;
            f->pipe = 0;
            f->major = 0;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;  // 文件表已满
}

// 增加文件引用计数（用于 dup）
struct file *filedup(struct file *f)
{
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("filedup");
    f->ref++;
    release(&ftable.lock);
    return f;
}

// 关闭文件，减少引用计数
void fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0)  // 还有其他进程在用，不真正关闭
    {
        release(&ftable.lock);
        return;
    }

    // 引用计数归零，真正关闭文件
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    f->readable = 0;
    f->writable = 0;
    f->off = 0;
    f->ip = 0;
    f->pipe = 0;
    f->major = 0;
    release(&ftable.lock);

    // 根据文件类型做清理
    if (ff.type == FD_PIPE)
    {
        pipeclose(ff.pipe, ff.writable);
    }
    else if (ff.type == FD_INODE)
    {
        begin_transaction();
        iput(ff.ip);
        end_transaction();
    }
}

// 获取文件状态信息
int filestat(struct file *f, uint64 addr)
{
    if (f->type == FD_DEVICE)  // 设备文件
    {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.type = T_DEVICE;
        st.dev = f->major;
        struct proc *p = myproc();
        if (copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
            return -1;
        return 0;
    }

    if (f->type != FD_INODE)  // 只支持普通文件和设备
        return -1;
    struct proc *p = myproc();
    struct stat st;
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if (copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
        return -1;
    return 0;
}

// 从文件读取数据
int fileread(struct file *f, uint64 addr, int n)
{
    if (!f->readable)
        return -1;

    if (f->type == FD_PIPE)  // 从管道读
        return piperead(f->pipe, addr, n);
    
    if (f->type == FD_DEVICE)  // 从设备读
    {
        if (f->major == CONSOLE)  // 控制台输入
        {
            int i = 0;
            while (i < n)
            {
                char c;
                int r = console_read(&c, 1);
                if (r < 0)
                    return i == 0 ? -1 : i;
                if (r == 0)
                    break;
                if (either_copyout(1, addr + i, &c, 1) < 0)
                    break;
                i += r;
                if (c == '\n')  // 读到换行符就返回
                    break;
            }
            return i;
        }
        return -1;
    }
    
    if (f->type == FD_INODE)  // 从普通文件读
    {
        ilock(f->ip);
        int r = readi(f->ip, 1, addr, f->off, n);
        if (r > 0)
            f->off += r;  // 更新文件偏移
        iunlock(f->ip);
        return r;
    }
    panic("fileread");
    return -1;
}

// 向文件写入数据
int filewrite(struct file *f, uint64 addr, int n)
{
    if (!f->writable) {
        printf("filewrite: not writable type=%d\n", f->type);
        return -1;
    }

    if (f->type == FD_PIPE) {  // 写入管道
        // 诊断：确认走到了管道写路径
        // printf("filewrite: FD_PIPE write n=%d\n", n);
        return pipewrite(f->pipe, addr, n);
    }

    if (f->type == FD_DEVICE)  // 写入设备
    {
        if (f->major == CONSOLE)  // 控制台输出
        {
            int i = 0;
            while (i < n)
            {
                char c;
                if (either_copyin(&c, 1, addr + i, 1) < 0)
                    break;
                console_write(&c, 1);
                i++;
            }
            return i;
        }
        return -1;
    }

    if (f->type == FD_INODE)  // 写入普通文件
    {
        // 计算单次事务能写入的最大字节数，避免日志溢出
        // MAXOPBLOCKS 是日志系统支持的最大操作块数
        // 减去的块：1个日志头块 + 1个inode块 + 2个间接块（bitmap和inode bitmap）
        // 除以2：因为每个数据块需要2个日志项（一个记录修改前，一个记录修改后）
        // 乘以BSIZE：转换为字节数
        int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
        int i = 0;
        while (i < n)
        {
            int n1 = n - i;
            if (n1 > max)
                n1 = max;

            begin_transaction();
            ilock(f->ip);
            int r = writei(f->ip, 1, addr + i, f->off, n1);
            if (r > 0)
                f->off += r;  // 更新文件偏移
            iunlock(f->ip);
            end_transaction();

            if (r < 0)
                break;
            if (r != n1)
                panic("short filewrite");
            i += r;
        }
        return i == n ? n : -1;
    }
    panic("filewrite");
    return -1;
}
