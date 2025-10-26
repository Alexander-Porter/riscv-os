#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "global_func.h"
#include "pipe.h"
#include "file.h"

// 管道实现：环形缓冲区 + 自旋锁 + sleep/wakeup

static int pipeclose_locked(struct pipe *p, int writable)
{
    if (writable)
    {
        p->writeopen = 0;
        wakeup(&p->nread); // 唤醒等待写端的读者
    }
    else
    {
        p->readopen = 0;
        wakeup(&p->nwrite); // 唤醒等待读端的写者
    }

    if (p->readopen == 0 && p->writeopen == 0)
    {
        release(&p->lock);
        kfree(p);
        return 0;
    }
    release(&p->lock);
    return 0;
}

int pipealloc(struct file **f0, struct file **f1)
{
    struct pipe *p = 0;
    *f0 = *f1 = 0;

    if ((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
        goto bad;

    if ((p = (struct pipe *)kmalloc(sizeof(*p))) == 0)
        goto bad;

    p->readopen = 1;
    p->writeopen = 1;
    p->nread = 0;
    p->nwrite = 0;
    initlock(&p->lock, "pipe");

    (*f0)->type = FD_PIPE;
    (*f0)->readable = 1;
    (*f0)->writable = 0;
    (*f0)->pipe = p;

    (*f1)->type = FD_PIPE;
    (*f1)->readable = 0;
    (*f1)->writable = 1;
    (*f1)->pipe = p;

    return 0;

bad:
    if (p)
        kfree(p);
    if (*f0)
        fileclose(*f0);
    if (*f1)
        fileclose(*f1);
    return -1;
}

void pipeclose(struct pipe *p, int writable)
{
    acquire(&p->lock);
    pipeclose_locked(p, writable);
}

int pipewrite(struct pipe *p, uint64 addr, int n)
{
    int i = 0;
    struct proc *pr = myproc();
    acquire(&p->lock);
    while (i < n)
    {
        if (p->readopen == 0 || killed(pr))
        {
            release(&p->lock);
            return -1;
        }
        if ((p->nwrite - p->nread) == BSIZE)
        {
            wakeup(&p->nread);
            sleep(&p->nwrite, &p->lock);
        }
        else
        {
            char ch;
            if (copyin(pr->pagetable, &ch, addr + i, 1) < 0)
                break;
            p->data[p->nwrite++ % BSIZE] = ch;
            i++;
        }
    }
    wakeup(&p->nread);
    release(&p->lock);
    return i;
}

int piperead(struct pipe *p, uint64 addr, int n)
{
    int i;
    struct proc *pr = myproc();
    acquire(&p->lock);
    while (p->nread == p->nwrite && p->writeopen)
    {
        if (killed(pr))
        {
            release(&p->lock);
            return -1;
        }
        sleep(&p->nread, &p->lock);
    }
    for (i = 0; i < n; i++)
    {
        if (p->nread == p->nwrite)
            break;
        char ch = p->data[p->nread++ % BSIZE];
        if (copyout(pr->pagetable, addr + i, &ch, 1) < 0)
            break;
    }
    wakeup(&p->nwrite);
    release(&p->lock);
    return i;
}
