#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "paging.h"
#include "proc.h"
#include "global_func.h"
#include "user_programs.h"
#include "shm.h"

extern volatile uint64 ticks;

static void free_kargv(char *kargv[], int count)
{
    for (int i = 0; i < count; i++)
    {
        if (kargv[i])
            kfree(kargv[i]);
    }
}

static int install_user_program(struct proc *p, const struct user_program *prog, char *kargv[], int argc)
{
    shm_cleanup_process(p);

    pagetable_t pagetable = proc_pagetable(p);
    if (pagetable == 0)
        return -1;

    uint64 prog_size = (uint64)(prog->end - prog->start);
    uint64 mapped_sz = 0;

    for (uint64 off = 0; off < prog_size || (prog_size == 0 && off == 0); off += PGSIZE)
    {
        char *mem = alloc_page();
        if (mem == 0)
            goto load_fail;
        memset(mem, 0, PGSIZE);

        uint64 copy_sz = PGSIZE;
        if (prog_size > 0)
        {
            if (prog_size - off < PGSIZE)
                copy_sz = prog_size - off;
            memmove(mem, prog->start + off, copy_sz);
        }

        if (mappages(pagetable, off, PGSIZE, (uint64)mem, PTE_R | PTE_W | PTE_X | PTE_U) != 0)
        {
            free_page(mem);
            goto load_fail;
        }
        mapped_sz = off + PGSIZE;
        if (prog_size == 0)
            break;
    }


    uint64 sz = PGROUNDUP(mapped_sz > 0 ? mapped_sz : prog_size);
    if (sz == 0)
        sz = PGSIZE;

    uint64 newsz = uvmalloc(pagetable, sz, sz + (USERSTACK_PAGES + 1) * PGSIZE);
    if (newsz == 0)
        goto load_fail;
    uvmclear(pagetable, newsz - (USERSTACK_PAGES + 1) * PGSIZE);
    mapped_sz = newsz;

    uint64 sp = newsz;
    uint64 stackbase = sp - USERSTACK_PAGES * PGSIZE;
    uint64 ustack[MAXARG + 1];

    for (int i = 0; i < argc; i++)
    {
        int len = strlen(kargv[i]) + 1;
        sp -= len;
        sp &= ~((uint64)15);
        if (sp < stackbase)
            goto load_fail;
        if (copyout(pagetable, sp, kargv[i], len) < 0)
            goto load_fail;
        ustack[i] = sp;
    }
    ustack[argc] = 0;

    sp -= (argc + 1) * sizeof(uint64);
    sp &= ~((uint64)15);
    if (sp < stackbase)
        goto load_fail;
    if (copyout(pagetable, sp, ustack, (argc + 1) * sizeof(uint64)) < 0)
        goto load_fail;

    struct trapframe *tf = p->trapframe;
    tf->epc = 0;
    tf->sp = sp;
    tf->a0 = argc;
    tf->a1 = sp;

    pagetable_t old = p->pagetable;
    uint64 oldsz = p->sz;

    p->pagetable = pagetable;
    p->sz = newsz;
    safestrcpy(p->name, prog->name, sizeof(p->name));
    p->time_slice = 0;
    p->ready_time = ticks;
    p->run_ticks = 0;

    proc_freepagetable(old, oldsz);
    return 0;

load_fail:
    proc_freepagetable(pagetable, mapped_sz);
    return -1;
}

int do_exec(uint64 path_addr, uint64 argv_addr)
{
    struct proc *p = myproc();
    char prog_name[PROG_NAME_MAX];

    if (copyinstr(p->pagetable, prog_name, path_addr, sizeof(prog_name)) < 0)
        return -1;

    const struct user_program *prog = find_user_program(prog_name);
    if (prog == 0)
    {
        printf("exec: program %s not found\n", prog_name);
        return -1;
    }

    char *kargv[MAXARG];
    for (int i = 0; i < MAXARG; i++)
        kargv[i] = 0;

    int argc = 0;
    if (argv_addr != 0)
    {
        for (; argc < MAXARG; argc++)
        {
            uint64 uarg;
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

    if (argc == MAXARG)
    {
        free_kargv(kargv, argc);
        return -1;
    }

    int rc = install_user_program(p, prog, kargv, argc);
    free_kargv(kargv, argc);
    return rc;
}

int exec_program_for_proc(struct proc *p, const char *name, char *argv[], int argc)
{
    const struct user_program *prog = find_user_program(name);
    if (prog == 0)
        return -1;
    return install_user_program(p, prog, argv, argc);
}

int do_exec_static(const char *name, char *argv[], int argc)
{
    return exec_program_for_proc(myproc(), name, argv, argc);
}
