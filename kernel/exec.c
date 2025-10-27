#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "paging.h"
#include "proc.h"
#include "global_func.h"
#include "fs.h"
#include "file.h"
#include "elf.h"
#include "shm.h"
#include "exec.h"
#include "user_programs.h"

// 从路径中提取末尾的程序名，忽略多余的斜杠
static const char *path_basename(const char *path)
{
    if (path == 0)
        return 0;
    while (*path == '/')
        path++;
    const char *last = path;
    for (const char *p = path; *p; p++)
        if (*p == '/')
            last = p + 1;
    return last;
}

// 从内置的用户程序镜像加载并运行程序
static int exec_from_binary(struct proc *p, const char *progname, const uchar *image, uint64 image_sz, uint64 entry, char *argv[], int argc)
{
    if (p == 0 || image == 0 || image_sz == 0)
        return -1;
    if (argc < 0 || argc > MAXARG)
        return -1;

    shm_cleanup_process(p);

    pagetable_t pagetable = proc_pagetable(p);
    if (pagetable == 0)
        return -1;

    uint64 sz = 0;
    uint64 alloc_sz = PGROUNDUP(image_sz);
    if (alloc_sz == 0)
        goto bad;

    uint64 sz1 = uvmalloc(pagetable, 0, alloc_sz, PTE_R | PTE_W | PTE_X | PTE_U);
    if (sz1 == 0)
        goto bad;
    sz = sz1;

    for (uint64 off = 0; off < image_sz;)
    {
        uint64 pa = walkaddr(pagetable, off);
        if (pa == 0)
            goto bad;
        uint64 n = PGSIZE - (off % PGSIZE);
        if (n > image_sz - off)
            n = image_sz - off;
        memmove((void *)(pa + (off % PGSIZE)), image + off, n);
        off += n;
    }

    sz = PGROUNDUP(sz);
    sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK_PAGES + 1) * PGSIZE, PTE_R | PTE_W | PTE_U);
    if (sz1 == 0)
        goto bad;
    uvmclear(pagetable, sz1 - (USERSTACK_PAGES + 1) * PGSIZE);

    uint64 sp = sz1;
    uint64 stackbase = sp - USERSTACK_PAGES * PGSIZE;
    uint64 ustack[MAXARG + 1];

    for (int i = argc - 1; i >= 0; i--)
    {
        if (argv == 0 || argv[i] == 0)
            goto bad;
        int len = strlen(argv[i]) + 1;
        sp -= len;
        sp &= ~((uint64)15);
        if (sp < stackbase)
            goto bad;
        if (copyout(pagetable, sp, argv[i], len) < 0)
            goto bad;
        ustack[i] = sp;
    }
    ustack[argc] = 0;

    sp -= (argc + 1) * sizeof(uint64);
    sp &= ~((uint64)15);
    if (sp < stackbase)
        goto bad;
    if (copyout(pagetable, sp, ustack, (argc + 1) * sizeof(uint64)) < 0)
        goto bad;

    struct trapframe *tf = p->trapframe;
    uint64 oldsz = p->sz;
    pagetable_t old = p->pagetable;

    tf->epc = entry;
    tf->sp = sp;
    tf->a0 = argc;
    tf->a1 = sp;

    p->pagetable = pagetable;
    p->sz = sz1;
    if (progname)
        safestrcpy(p->name, progname, sizeof(p->name));

    proc_freepagetable(old, oldsz);
    return 0;

bad:
    if (pagetable)
        proc_freepagetable(pagetable, sz);
    return -1;
}

static int flags2perm(int flags)
{
    int perm = PTE_U;
    if (flags & 0x1)
        perm |= PTE_X;
    if (flags & 0x2)
        perm |= PTE_W;
    return perm | PTE_R;
}

static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
    for (uint i = 0; i < sz; i += PGSIZE)
    {
        uint64 pa = walkaddr(pagetable, va + i);
        if (pa == 0)
            panic("loadseg: address should exist");
        uint n = PGSIZE;
        if (sz - i < PGSIZE)
            n = sz - i;
        if (readi(ip, 0, pa, offset + i, n) != (int)n)
            return -1;
    }
    return 0;
}

static void free_kargv(char *kargv[], int count)
{
    for (int i = 0; i < count; i++)
    {
        if (kargv[i])
            kfree(kargv[i]);
    }
}

int do_exec(uint64 path_addr, uint64 argv_addr)
{
    struct proc *p = myproc();
    char path[MAXPATH];
    if (copyinstr(p->pagetable, path, path_addr, sizeof(path)) < 0)
        return -1;

    char *kargv[MAXARG];
    for (int i = 0; i < MAXARG; i++)
        kargv[i] = 0;

    int argc = 0;
    if (argv_addr != 0)
    {
        for (; argc < MAXARG; argc++)
        {
            uint64 uarg = 0;
            if (copyin(p->pagetable, &uarg, argv_addr + argc * sizeof(uint64), sizeof(uint64)) < 0)
            {
                free_kargv(kargv, argc);
                return -1;
            }
            if (uarg == 0)
                break;
            kargv[argc] = (char *)kmalloc(MAXARGLEN);
            if (kargv[argc] == 0)
            {
                free_kargv(kargv, argc);
                return -1;
            }
            if (copyinstr(p->pagetable, kargv[argc], uarg, MAXARGLEN) < 0)
            {
                free_kargv(kargv, argc + 1);
                return -1;
            }
        }
    }
    kargv[argc] = 0;

    begin_transaction();
    struct inode *ip = path_walk(path);
    if (ip == 0)
    {
    end_transaction();
        const char *base = path_basename(path);
        int r = do_exec_static(base ? base : path, kargv, argc);
        free_kargv(kargv, argc);
        return r;
    }

    ilock(ip);
    struct elfhdr elf;
    if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
        goto bad;
    if (elf.magic != ELF_MAGIC)
        goto bad;

    shm_cleanup_process(p);

    pagetable_t pagetable = proc_pagetable(p);
    if (pagetable == 0)
        goto bad;

    uint64 sz = 0;
    struct proghdr ph;
    for (int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph))
    {
        if (readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
            goto load_bad;
        if (ph.type != ELF_PROG_LOAD)
            continue;
        if (ph.memsz < ph.filesz)
            goto load_bad;
        if (ph.vaddr + ph.memsz < ph.vaddr)
            goto load_bad;
        if (ph.vaddr % PGSIZE != 0)
            goto load_bad;
        uint64 sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags));
        if (sz1 == 0)
            goto load_bad;
        sz = sz1;
        if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
            goto load_bad;
    }

    iunlockput(ip);
    end_transaction();
    ip = 0;

    sz = PGROUNDUP(sz);
    uint64 sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK_PAGES + 1) * PGSIZE, PTE_W | PTE_R | PTE_U);
    if (sz1 == 0)
        goto load_bad_cleanup;
    sz = sz1;
    uvmclear(pagetable, sz - (USERSTACK_PAGES + 1) * PGSIZE);

    uint64 sp = sz;
    uint64 stackbase = sp - USERSTACK_PAGES * PGSIZE;
    uint64 ustack[MAXARG + 1];

    for (int i = argc - 1; i >= 0; i--)
    {
        int len = strlen(kargv[i]) + 1;
        sp -= len;
        sp &= ~((uint64)15);
        if (sp < stackbase)
            goto load_bad_cleanup;
        if (copyout(pagetable, sp, kargv[i], len) < 0)
            goto load_bad_cleanup;
        ustack[i] = sp;
    }
    ustack[argc] = 0;

    sp -= (argc + 1) * sizeof(uint64);
    sp &= ~((uint64)15);
    if (sp < stackbase)
        goto load_bad_cleanup;
    if (copyout(pagetable, sp, ustack, (argc + 1) * sizeof(uint64)) < 0)
        goto load_bad_cleanup;

    struct trapframe *tf = p->trapframe;
    uint64 oldsz = p->sz;
    pagetable_t old = p->pagetable;

    tf->epc = elf.entry;
    tf->sp = sp;
    tf->a0 = argc;
    tf->a1 = sp;

    p->pagetable = pagetable;
    p->sz = sz;
    safestrcpy(p->name, path, sizeof(p->name));

    proc_freepagetable(old, oldsz);
    free_kargv(kargv, argc);
    return 0;

load_bad:
    proc_freepagetable(pagetable, sz);
load_bad_cleanup:
    if (ip)
    {
        iunlockput(ip);
    end_transaction();
    }
    free_kargv(kargv, argc);
    return -1;

bad:
    iunlockput(ip);
    end_transaction();
    free_kargv(kargv, argc);
    return -1;
}

int exec_program_for_proc(struct proc *p, const char *name, char *argv[], int argc)
{
    if (p == 0 || name == 0)
        return -1;
    if (argc < 0 || argc > MAXARG)
        return -1;
    if (argc > 0 && argv == 0)
        return -1;

    const struct user_program *prog = find_user_program(name);
    if (prog == 0)
        return -1;

    uint64 image_sz = prog->end - prog->start;
    if (image_sz == 0)
        return -1;

    return exec_from_binary(p, name, prog->start, image_sz, 0, argv, argc);
}

int do_exec_static(const char *name, char *argv[], int argc)
{
    struct proc *p = myproc();
    if (p == 0 || name == 0)
        return -1;
    if (argc < 0 || argc > MAXARG)
        return -1;
    if (argc > 0 && argv == 0)
        return -1;

    const char *base = path_basename(name);
    if (base == 0 || *base == '\0')
        base = name;

    const struct user_program *prog = find_user_program(base);
    if (prog == 0)
        return -1;

    uint64 image_sz = prog->end - prog->start;
    if (image_sz == 0)
        return -1;

    return exec_from_binary(p, base, prog->start, image_sz, 0, argv, argc);
}
