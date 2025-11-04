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

struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

void fileinit(void)
{
    initlock(&ftable.lock, "ftable");
}

struct file *filealloc(void)
{
    acquire(&ftable.lock);
    for (struct file *f = ftable.file; f < ftable.file + NFILE; f++)
    {
        if (f->ref == 0)
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
    return 0;
}

struct file *filedup(struct file *f)
{
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("filedup");
    f->ref++;
    release(&ftable.lock);
    return f;
}

void fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0)
    {
        release(&ftable.lock);
        return;
    }

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

int filestat(struct file *f, uint64 addr)
{
    if (f->type == FD_DEVICE)
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

    if (f->type != FD_INODE)
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

int fileread(struct file *f, uint64 addr, int n)
{
    if (!f->readable)
        return -1;

    if (f->type == FD_PIPE)
        return piperead(f->pipe, addr, n);
    if (f->type == FD_DEVICE)
    {
        if (f->major == CONSOLE)
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
                if (c == '\n')
                    break;
            }
            return i;
        }
        return -1;
    }
    if (f->type == FD_INODE)
    {
        ilock(f->ip);
        int r = readi(f->ip, 1, addr, f->off, n);
        if (r > 0)
            f->off += r;
        iunlock(f->ip);
        return r;
    }
    panic("fileread");
    return -1;
}

int filewrite(struct file *f, uint64 addr, int n)
{
    if (!f->writable) {
        printf("filewrite: not writable type=%d\n", f->type);
        return -1;
    }

    if (f->type == FD_PIPE) {
        // 诊断：确认走到了管道写路径
        // printf("filewrite: FD_PIPE write n=%d\n", n);
        return pipewrite(f->pipe, addr, n);
    }

    if (f->type == FD_DEVICE)
    {
        if (f->major == CONSOLE)
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

    if (f->type == FD_INODE)
    {
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
                f->off += r;
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
