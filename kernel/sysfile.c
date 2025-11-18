#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "stat.h"
#include "syscall.h"
#include "global_func.h"
#include "pipe.h"

static struct file *argfile(int n)
{
    int fd;
    if (argint(n, &fd) < 0)
        return 0;
    struct proc *p = myproc();
    if (fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
        return 0;
    return p->ofile[fd];
}

static int fdalloc(struct file *f)
{
    struct proc *p = myproc();
    for (int fd = 0; fd < NOFILE; fd++)
    {
        if (p->ofile[fd] == 0)
        {
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

uint64 sys_dup(void)
{
    struct file *f = argfile(0);
    if (f == 0)
        return -1;
    struct file *nf = filedup(f);
    int fd = fdalloc(nf);
    if (fd < 0)
    {
        fileclose(nf);
        return -1;
    }
    return fd;
}

uint64 sys_read(void)
{
    struct file *f = argfile(0);
    if (f == 0)
        return -1;
    uint64 addr;
    int n;
    if (argaddr(1, &addr) < 0 || argint(2, &n) < 0)
        return -1;
    return fileread(f, addr, n);
}

uint64 sys_write(void)
{
    struct file *f = argfile(0);
    if (f == 0)
        return -1;
    uint64 addr;
    int n;
    if (argaddr(1, &addr) < 0 || argint(2, &n) < 0)
        return -1;
    return filewrite(f, addr, n);
}

uint64 sys_close(void)
{
    int fd;
    if (argint(0, &fd) < 0)
        return -1;
    struct proc *p = myproc();
    if (fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
        return -1;
    struct file *f = p->ofile[fd];
    p->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

uint64 sys_fstat(void)
{
    struct file *f = argfile(0);
    if (f == 0)
        return -1;
    uint64 addr;
    if (argaddr(1, &addr) < 0)
        return -1;
    return filestat(f, addr);
}

static struct inode *create(char *path, short type, short major, short minor)
{
    struct inode *ip;
    struct inode *dp;
    char name[DIRSIZ];

    if ((dp = path_parent(path, name)) == 0)
        return 0;

    ilock(dp);

    if ((ip = dir_lookup(dp, name, 0)) != 0)
    {
        iunlockput(dp);
        ilock(ip);
        if (type == T_FILE && ip->type == T_FILE)
            return ip;
        iput(ip);
        return 0;
    }

    if ((ip = ialloc(dp->dev, type)) == 0)
        panic("create: ialloc");

    ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    iupdate(ip);

    if (type == T_DIR)
    {
        dp->nlink++;
        iupdate(dp);

    if (dir_link(ip, ".", ip->inum) < 0 || dir_link(ip, "..", dp->inum) < 0)
            panic("create dots");
    }

    if (dir_link(dp, name, ip->inum) < 0)
        panic("create: dirlink");

    iunlockput(dp);
    return ip;
}

uint64 sys_open(void)
{
    char path[MAXPATH];
    int omode;
    if (argstr(0, path, sizeof(path)) < 0 || argint(1, &omode) < 0)
        return -1;

    // 可选调试：跟踪 open 调用路径（默认关闭）
    // printf("sys_open: path=%s omode=0x%x\n", path, omode);

    begin_transaction();

    struct inode *ip;
    if (omode & O_CREATE)
    {
    // printf("sys_open: create %s\n", path);
        ip = create(path, T_FILE, 0, 0);
        if (ip == 0)
        {
            // printf("sys_open: create failed for %s\n", path);
            end_transaction();
            return -1;
        }
    }
    else
    {
    if ((ip = path_walk(path)) == 0)
        {
            end_transaction();
            return -1;
        }
        ilock(ip);
        if (ip->type == T_DIR && omode != O_RDONLY)
        {
            iunlockput(ip);
            end_transaction();
            return -1;
        }
    }

    if ((ip->type == T_DEVICE) && (ip->major < 0 || ip->major >= NDEV))
    {
        iunlockput(ip);
    end_transaction();
    printf("sys_open: filealloc failed for %s\n", path);
    return -1;
    }

    struct file *f = filealloc();
    if (f == 0)
    {
        iunlockput(ip);
    end_transaction();
        return -1;
    }

    int fd = fdalloc(f);
    if (fd < 0)
    {
        fileclose(f);
        iunlockput(ip);
    end_transaction();
    // printf("sys_open: fdalloc failed for %s\n", path);
        return -1;
    }

    f->type = (ip->type == T_DEVICE) ? FD_DEVICE : FD_INODE;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR) || (omode & O_TRUNC);
    f->ip = ip;
    f->off = 0;
    if (ip->type == T_DEVICE)
        f->major = ip->major;

    if (omode & O_TRUNC)
        itrunc(ip);

    iunlock(ip);
    end_transaction();
    // printf("sys_open: ok fd=%d %s\n", fd, path);
    return fd;
}

uint64 sys_mkdir(void)
{
    char path[MAXPATH];
    if (argstr(0, path, sizeof(path)) < 0)
        return -1;
    begin_transaction();
    struct inode *ip = create(path, T_DIR, 0, 0);
    if (ip == 0)
    {
        end_transaction();
        return -1;
    }
    iunlockput(ip);
    end_transaction();
    return 0;
}

uint64 sys_chdir(void)
{
    char path[MAXPATH];
    struct proc *p = myproc();
    if (argstr(0, path, sizeof(path)) < 0)
        return -1;

    begin_transaction();
    struct inode *ip = path_walk(path);
    if (ip == 0)
    {
        end_transaction();
        return -1;
    }
    ilock(ip);
    if (ip->type != T_DIR)
    {
        iunlockput(ip);
        end_transaction();
        return -1;
    }
    iunlock(ip);
    iput(p->cwd);
    p->cwd = ip;
    end_transaction();
    return 0;
}

uint64 sys_symlink(void)
{
    char target[MAXPATH], linkpath[MAXPATH];
    if (argstr(0, target, sizeof(target)) < 0 || argstr(1, linkpath, sizeof(linkpath)) < 0)
        return -1;

    int tlen = strlen(target);
    if (tlen + 1 > MAXPATH)
        return -1;

    begin_transaction();
    struct inode *ip = create(linkpath, T_SYMLINK, 0, 0);
    if (ip == 0)
    {
        end_transaction();
        return -1;
    }

    int write_len = tlen + 1;
    if (writei(ip, 0, (uint64)target, 0, write_len) != write_len)
    {
        iunlockput(ip);
        end_transaction();
        return -1;
    }

    iunlockput(ip);
    end_transaction();
    return 0;
}

uint64 sys_link(void)
{
    char old[MAXPATH], new[MAXPATH];
    if (argstr(0, old, sizeof(old)) < 0 || argstr(1, new, sizeof(new)) < 0)
        return -1;

    begin_transaction();
    struct inode *ip = path_walk(old);
    if (ip == 0)
    {
    end_transaction();
        return -1;
    }
    ilock(ip);
    if (ip->type == T_DIR)
    {
        iunlockput(ip);
    end_transaction();
        return -1;
    }
    ip->nlink++;
    iupdate(ip);
    iunlock(ip);

    char name[DIRSIZ];
    struct inode *dp = path_parent(new, name);
    if (dp == 0)
        goto bad;
    ilock(dp);
    if (dp->dev != ip->dev || dir_link(dp, name, ip->inum) < 0)
    {
        iunlockput(dp);
        goto bad;
    }
    iunlockput(dp);
    iput(ip);
    end_transaction();
    return 0;

bad:
    ilock(ip);
    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);
    end_transaction();
    return -1;
}

static int isdirempty(struct inode *dp)
{
    struct dirent de;
    for (uint off = 2 * sizeof(de); off < dp->size; off += sizeof(de))
    {
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty");
        if (de.inum != 0)
            return 0;
    }
    return 1;
}

uint64 sys_readlink(void)
{
    char path[MAXPATH];
    uint64 bufaddr;
    int buflen;
    if (argstr(0, path, sizeof(path)) < 0 || argaddr(1, &bufaddr) < 0 || argint(2, &buflen) < 0)
        return -1;
    if (buflen <= 0)
        return -1;

    struct inode *ip = path_walk_nofollow(path);
    if (ip == 0)
        return -1;

    ilock(ip);
    if (ip->type != T_SYMLINK)
    {
        iunlockput(ip);
        return -1;
    }

    int len = ip->size;
    if (len > MAXPATH)
        len = MAXPATH;
    char target[MAXPATH];
    int n = readi(ip, 0, (uint64)target, 0, len);
    if (n < 0)
    {
        iunlockput(ip);
        return -1;
    }
    if (n > 0 && target[n - 1] == '\0')
        n--;
    if (n > buflen)
        n = buflen;
    if (either_copyout(1, bufaddr, target, n) < 0)
    {
        iunlockput(ip);
        return -1;
    }
    iunlockput(ip);
    return n;
}

uint64 sys_unlink(void)
{
    char path[MAXPATH];
    if (argstr(0, path, sizeof(path)) < 0)
        return -1;

    // 注：保留安静，避免影响性能测试输出

    begin_transaction();
    char name[DIRSIZ];
    struct inode *dp = path_parent(path, name);
    if (dp == 0)
    {
    end_transaction();
    return -1;
    }

    ilock(dp);
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        iunlockput(dp);
    end_transaction();
    return -1;
    }

    uint off;
    struct inode *ip = dir_lookup(dp, name, &off);
    if (ip == 0)
    {
        iunlockput(dp);
    end_transaction();
    return -1;
    }
    ilock(ip);
    if (ip->nlink < 1)
        panic("unlink: nlink < 1");
    if (ip->type == T_DIR && !isdirempty(ip))
    {
        iunlockput(ip);
        iunlockput(dp);
    end_transaction();
        return -1;
    }

    struct dirent de = {0};
    if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        panic("unlink: writei");

    if (ip->type == T_DIR)
    {
        dp->nlink--;
        iupdate(dp);
    }

    iunlockput(dp);

    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);
    end_transaction();
    return 0;
}

uint64 sys_pipe(void)
{
    uint64 fdarray;
    if (argaddr(0, &fdarray) < 0)
        return -1;

    struct file *rf, *wf;
    if (pipealloc(&rf, &wf) < 0)
        return -1;

    int fd0 = fdalloc(rf);
    int fd1 = fdalloc(wf);
    if (fd0 < 0 || fd1 < 0)
    {
        if (fd0 >= 0)
            myproc()->ofile[fd0] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }

    // 注意：用户态 pipe 原型为 int fds[2]，因此这里只能拷贝 2 个 int，

    int fds[2] = {fd0, fd1};
    if (copyout(myproc()->pagetable, fdarray, fds, sizeof(int) * 2) < 0)
    {
        myproc()->ofile[fd0] = 0;
        myproc()->ofile[fd1] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    return 0;
}