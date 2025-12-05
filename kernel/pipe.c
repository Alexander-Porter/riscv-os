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
        if (p->wcount > 0)
            p->wcount--;
        wakeup(&p->nread); // 唤醒等待写端的读者
    }
    else
    {
        if (p->rcount > 0)
            p->rcount--;
        wakeup(&p->nwrite); // 唤醒等待读端的写者
    }

    if (p->rcount == 0 && p->wcount == 0)
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

    p->rcount = 1;
    p->wcount = 1;
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
    struct proc *pr = myproc(); // 获取当前进程结构体

    acquire(&p->lock); // 【加锁】进入临界区，保证同一时间只有一个进程操作该管道

    while (i < n)
    {

        // 如果没有读者 (rcount == 0)，写入也没有意义（Broken Pipe），通常对应 SIGPIPE 信号
        // 或者当前进程被杀死了 (killed)
        if (p->rcount == 0 || killed(pr))
        {
            // 诊断输出，定位为何写入失败
            printf("pipewrite: rcount=%d killed=%d\n", p->rcount, killed(pr));
            release(&p->lock); // 出错返回前必须释放锁
            return -1;         // 返回错误
        }


        // nwrite 是累计写入字节数，nread 是累计读取字节数
        // 差值等于当前缓冲区内的数据量。BSIZE 是缓冲区总大小。
        if ((p->nwrite - p->nread) == BSIZE)
        {
            // 缓冲区满了，无法写入：
            wakeup(&p->nread);           // 唤醒可能正在休眠的读者（告诉它们“快来读，我写不下了”）
            sleep(&p->nwrite, &p->lock); // 自己进入休眠（释放锁，等待被唤醒），等待 nwrite 变化（即有空间）
        }
        else
        {
            // 3. 写入数据
            char ch;
            // 从用户空间 (addr + i) 拷贝 1 字节数据到内核变量 ch
            if (copyin(pr->pagetable, &ch, addr + i, 1) < 0)
                break; // 拷贝失败（如非法地址），退出循环

            // 写入环形缓冲区
            // 使用取模运算 (%) 实现循环队列
            p->data[p->nwrite++ % BSIZE] = ch; 
            i++; // 已写入字节数 +1
        }
    }

    wakeup(&p->nread); // 写完了（或写了一部分），唤醒读者来读数据
    release(&p->lock); // 【解锁】离开临界区
    return i;          // 返回实际写入的字节数
}

int piperead(struct pipe *p, uint64 addr, int n)
{
    int i;
    struct proc *pr = myproc(); // 获取当前进程

    acquire(&p->lock); // 【加锁】保护管道数据


    // 条件：缓冲区为空 (nread == nwrite) 且 还有写者在线 (wcount > 0)
    while (p->nread == p->nwrite && p->wcount > 0)
    {
        if (killed(pr)) // 如果进程被杀死
        {
            release(&p->lock);
            return -1;
        }
        // 缓冲区空了，进入休眠，等待写者（nread 通道）唤醒
        sleep(&p->nread, &p->lock); 
    }

    // 2. 读取数据
    for (i = 0; i < n; i++)
    {
        // 如果读着读着缓冲区空了，就停止读取
        // 注意：这里不会再次 sleep，因为我们要尽可能返回已读到的数据，而不是无限等待
        if (p->nread == p->nwrite)
            break;

        // 从环形缓冲区取出一个字节
        char ch = p->data[p->nread++ % BSIZE];

        // 将数据拷贝回用户空间 (addr + i)
        if (copyout(pr->pagetable, addr + i, &ch, 1) < 0)
            break; // 拷贝失败
    }

    wakeup(&p->nwrite); // 读走了一些数据，腾出了空间，唤醒可能因“缓冲区满”而休眠的写者
    release(&p->lock);  
    return i;           // 返回实际读取的字节数
}
