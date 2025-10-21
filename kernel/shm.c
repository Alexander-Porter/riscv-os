#include "shm.h"
#include "memlayout.h"
#include "spinlock.h"
#include "global_func.h"
#include "proc.h"
#include "paging.h"

struct shm_page {
    struct spinlock lock; // 保护当前共享页的元数据
    void *pa;             // 物理页地址
    int used;             // 是否被占用
    int refs;             // 已映射的进程数量
};

static struct {
    struct spinlock lock;           // 全局表锁，用于分配 slot
    struct shm_page pages[SHM_MAX_PAGES];
} shm_table;

static inline uint64 shm_slot_va(int shmid)
{
    return SHM_BASE + (uint64)shmid * PGSIZE;
}

uint64 shm_va_for_id(int shmid)
{
    if (shmid < 0 || shmid >= SHM_MAX_PAGES)
        return 0;
    return shm_slot_va(shmid);
}

int shm_id_from_va(uint64 va)
{
    if (va < SHM_BASE || va >= SHM_TOP)
        return -1;
    if (va % PGSIZE)
        return -1;
    return (int)((va - SHM_BASE) / PGSIZE);
}

static struct shm_page *get_shm_page(int shmid)
{
    if (shmid < 0 || shmid >= SHM_MAX_PAGES)
        return 0;
    return &shm_table.pages[shmid];
}

static struct shm_mapping *proc_find_mapping(struct proc *p, int shmid)
{
    for (int i = 0; i < PROC_SHM_MAX; i++)
    {
        if (p->shm_regions[i].used && p->shm_regions[i].shmid == shmid)
            return &p->shm_regions[i];
    }
    return 0;
}

static struct shm_mapping *proc_find_mapping_by_va(struct proc *p, uint64 va)
{
    for (int i = 0; i < PROC_SHM_MAX; i++)
    {
        if (p->shm_regions[i].used && p->shm_regions[i].va == va)
            return &p->shm_regions[i];
    }
    return 0;
}

static struct shm_mapping *proc_allocate_mapping(struct proc *p)
{
    for (int i = 0; i < PROC_SHM_MAX; i++)
    {
        if (!p->shm_regions[i].used)
            return &p->shm_regions[i];
    }
    return 0;
}

void shm_system_init(void)
{
    initlock(&shm_table.lock, "shmtable");
    for (int i = 0; i < SHM_MAX_PAGES; i++)
    {
        initlock(&shm_table.pages[i].lock, "shmpage");
        shm_table.pages[i].pa = 0;
        shm_table.pages[i].used = 0;
        shm_table.pages[i].refs = 0;
    }
}

int shm_create(void)
{
    int slot = -1;

    acquire(&shm_table.lock);
    for (int i = 0; i < SHM_MAX_PAGES; i++)
    {
        if (!shm_table.pages[i].used)
        {
            shm_table.pages[i].used = 1;
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        release(&shm_table.lock);
        return -1;
    }
    struct shm_page *page = &shm_table.pages[slot];
    acquire(&page->lock);
    release(&shm_table.lock);

    void *pa = alloc_page();
    if (pa == 0)
    {
        page->used = 0;
        release(&page->lock);
        return -1;
    }
    memset(pa, 0, PGSIZE);
    page->pa = pa;
    page->refs = 0;
    release(&page->lock);

    return slot;
}

int shm_map_for_proc(struct proc *p, int shmid, uint64 *out_va)
{
    if (p == 0 || out_va == 0)
        return -1;

    struct shm_page *page = get_shm_page(shmid);
    if (page == 0)
        return -1;

    struct shm_mapping *existing = proc_find_mapping(p, shmid);
    if (existing)
    {
        *out_va = existing->va;
        return 0;
    }

    uint64 va = shm_va_for_id(shmid);
    if (va == 0)
        return -1;

    acquire(&page->lock);
    if (!page->used || page->pa == 0)
    {
        release(&page->lock);
        return -1;
    }

    if (walkaddr(p->pagetable, va) != 0)
    {
        release(&page->lock);
        return -1;
    }

    if (mappages(p->pagetable, va, PGSIZE, (uint64)page->pa, PTE_R | PTE_W | PTE_U | PTE_SHARED) != 0)
    {
        release(&page->lock);
        return -1;
    }

    incref_page(page->pa);
    page->refs++;
    release(&page->lock);

    struct shm_mapping *slot = proc_allocate_mapping(p);
    if (slot == 0)
    {
        uvmunmap(p->pagetable, va, 1, 0);
        free_page(page->pa);
        acquire(&page->lock);
        page->refs--;
        if (page->refs < 0)
            page->refs = 0;
        release(&page->lock);
        return -1;
    }

    slot->used = 1;
    slot->shmid = shmid;
    slot->va = va;
    p->shm_region_count++;

    *out_va = va;
    return 0;
}

int shm_unmap_for_proc(struct proc *p, uint64 va)
{
    if (p == 0)
        return -1;

    int shmid = shm_id_from_va(va);
    if (shmid < 0)
        return -1;

    struct shm_page *page = get_shm_page(shmid);
    if (page == 0)
        return -1;

    struct shm_mapping *slot = proc_find_mapping_by_va(p, va);
    if (slot == 0)
        return -1;

    acquire(&page->lock);
    if (!page->used || page->pa == 0)
    {
        release(&page->lock);
        return -1;
    }

    if (walkaddr(p->pagetable, va) == 0)
    {
        release(&page->lock);
        return -1;
    }

    uvmunmap(p->pagetable, va, 1, 0);
    free_page(page->pa);

    page->refs--;
    if (page->refs < 0)
        page->refs = 0;

    int should_release_slot = 0;
    void *base_pa = 0;
    if (page->refs == 0)
    {
        base_pa = page->pa;
        page->pa = 0;
        page->used = 0;
        should_release_slot = 1;
    }
    release(&page->lock);

    if (should_release_slot && base_pa)
        free_page(base_pa);

    slot->used = 0;
    slot->shmid = -1;
    slot->va = 0;
    if (p->shm_region_count > 0)
        p->shm_region_count--;

    return 0;
}

void shm_cleanup_process(struct proc *p)
{
    if (p == 0)
        return;

    for (int i = 0; i < PROC_SHM_MAX; i++)
    {
        if (p->shm_regions[i].used)
            shm_unmap_for_proc(p, p->shm_regions[i].va);
    }
}

int shm_clone_mappings(struct proc *src, struct proc *dst)
{
    if (src == 0 || dst == 0)
        return -1;

    for (int i = 0; i < PROC_SHM_MAX; i++)
    {
        if (!src->shm_regions[i].used)
            continue;
        uint64 va = 0;
        if (shm_map_for_proc(dst, src->shm_regions[i].shmid, &va) < 0)
        {
            shm_cleanup_process(dst);
            return -1;
        }
    }
    return 0;
}
