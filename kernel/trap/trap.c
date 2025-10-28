#include "../include/trap.h"
#include "../include/riscv.h"

#include "../global_func.h"
#include "../types.h"
#include "../paging.h"
#include "../memlayout.h"
#include "../proc.h"

// 声明外部函数
extern int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
extern void *alloc_page(void);
extern void free_page(void *pa);
extern void *memset(void *dst, int c, uint size);
extern char trampoline[];
extern char uservec[];
extern char userret[];
extern pagetable_t kernel_pagetable;
extern void syscall(void);

// 全局变量
volatile uint64 ticks = 0;
struct interrupt_desc *interrupt_table[MAX_IRQ_NUM];
static int nested_level = 0;                        // 嵌套中断层级
static int current_priority = IRQ_PRIORITY_LOW + 1; // 当前处理的中断优先级

static void handle_user_exception(struct proc *p, uint64 cause, uint64 epc, uint64 tval);
static int handle_cow_fault(struct proc *p, uint64 va);
#if ENABLE_LAZY_SBRK
static int handle_lazy_alloc(struct proc *p, uint64 va);
#endif


// 外部声明的汇编函数
extern void kernelvec(void);

/**
 * 初始化中断系统
 */
void trap_init(void)
{
    // 初始化中断向量表
    for (int i = 0; i < MAX_IRQ_NUM; i++)
    {
        interrupt_table[i] = 0;
    }

    printf("Trap system initialized\n");
}

/**
 * 初始化每个hart的中断处理
 */
void trap_init_hart(void)
{
    // 设置内核中断向量
    //1. Direct Mode（直接模式）
    //stvec 的低 2 位为 00
    //整个 stvec 寄存器指向一个单一的陷阱处理函数地址
    //2. Vectored Mode（向量模式）
    //stvec 的低 2 位为 01
    //stvec 的高位部分指向向量表的基地址
    //当陷阱发生时，硬件会计算：跳转地址 = stvec[63:2] + (scause × 4)
    w_stvec((uint64)kernelvec);

    // 启用监督模式下的中断
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
}

/**
 * 注册中断处理函数（类似Linux的request_irq）
 * @param irq 中断号
 * @param handler 处理函数
 * @param name 中断名称
 * @return 0成功，-1失败
 */
int register_interrupt(int irq, interrupt_handler_t handler, const char *name)
{
    if (irq < 0 || irq >= MAX_IRQ_NUM || !handler)
    {
        return -1;
    }

    // 分配新的中断描述符
    struct interrupt_desc *new_desc = (struct interrupt_desc *)kmalloc(sizeof(struct interrupt_desc));
    if (!new_desc)
    {
        return -1;
    }

    // 初始化中断描述符
    new_desc->handler = handler;
    new_desc->irq = irq;
    new_desc->next = 0;
    new_desc->prev = 0;

    // 复制名称
    int i;
    for (i = 0; i < 31 && name[i]; i++)
    {
        new_desc->name[i] = name[i];
    }
    new_desc->name[i] = '\0';

    // 插入到中断链表尾部，保持注册顺序（后注册的后调用）
    if (!interrupt_table[irq])
    {
        // 第一个中断处理函数
        interrupt_table[irq] = new_desc;
        new_desc->prev = new_desc; // 头节点的prev指向自己（尾节点）
    }
    else
    {
        // 从头节点的prev直接获取尾节点
        struct interrupt_desc *tail = interrupt_table[irq]->prev;
        // 插入到尾部
        tail->next = new_desc;
        new_desc->prev = tail;
        // 更新头节点的prev指向新的尾节点
        interrupt_table[irq]->prev = new_desc;
    }
    return 0;
}

/**
 * 注销中断处理函数
 */
void unregister_interrupt(int irq, interrupt_handler_t handler)
{
    if (irq < 0 || irq >= MAX_IRQ_NUM || !handler)
    {
        return;
    }

    struct interrupt_desc *current = interrupt_table[irq];

    while (current)
    {
        if (current->handler == handler)
        {
            // 检查是否是单节点情况（头节点的prev指向自己）
            if (current->prev == current && current->next == 0)
            {
                // 只有一个节点，直接清空链表
                interrupt_table[irq] = 0;
            }
            else
            {
                // 从链表中移除
                if (current->prev)
                {
                    current->prev->next = current->next;
                }
                else
                {
                    interrupt_table[irq] = current->next;
                }

                if (current->next)
                {
                    current->next->prev = current->prev;
                }

                // 如果删除的是尾节点，需要更新头节点的prev指针
                if (interrupt_table[irq]  && !current->next)
                {
                    interrupt_table[irq]->prev = current->prev;
                }
            }

            kfree(current);
            return;
        }
        current = current->next;
    }
}

/**
 * 启用特定中断
 */
void enable_interrupt(int irq)
{
    if (irq == IRQ_TIMER)
    {
        w_sie(r_sie() | SIE_STIE);
    }
    else if (irq == IRQ_EXTERNAL)
    {
        w_sie(r_sie() | SIE_SEIE);
    }
    else if (irq == IRQ_SOFTWARE)
    {
        w_sie(r_sie() | SIE_SSIE);
    }
}

/**
 * 禁用特定中断
 */
void disable_interrupt(int irq)
{
    if (irq == IRQ_TIMER)
    {
        w_sie(r_sie() & ~SIE_STIE);
    }
    else if (irq == IRQ_EXTERNAL)
    {
        w_sie(r_sie() & ~SIE_SEIE);
    }
    else if (irq == IRQ_SOFTWARE)
    {
        w_sie(r_sie() & ~SIE_SSIE);
    }
}

/**
 * 内核中断处理函数
 */
void kerneltrap(void)
{
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    // 检查是否来自监督模式
    if ((sstatus & SSTATUS_SPP) == 0)
    {
        panic("kerneltrap: not from supervisor mode");
    }

    // 检查中断是否被禁用
    if (intr_get() != 0)
    {
        panic("kerneltrap: interrupts enabled");
    }

    nested_level++;

    int which_dev = devintr();
    if (which_dev == 0)
    {
        // 与 xv6 一致：内核态异常一律视为致命错误，打印关键信息后 panic
        printf("kerneltrap: unexpected scause=0x%lx sepc=0x%lx stval=0x%lx\n",
               scause, sepc, r_stval());
        panic("kerneltrap");
    }
    // 注意：不要在内核态直接进行调度切换，避免持锁期间被抢占导致死锁/饥饿。
    // 仅在返回用户态(usertrap)时根据策略决定是否让出CPU。

    nested_level--;

    w_sepc(sepc);
    w_sstatus(sstatus);
}

// IRQ线的优先级定义（不同中断源的优先级）
static int irq_priorities[MAX_IRQ_NUM] = {
    [IRQ_SOFTWARE] = IRQ_PRIORITY_LOW,  // 软件中断优先级低
    [IRQ_TIMER] = IRQ_PRIORITY_NORMAL,  // 时钟中断中等优先级
    [IRQ_EXTERNAL] = IRQ_PRIORITY_HIGH, // 外部中断优先级高
};

/**
 * 处理特定中断号的所有注册处理函数，支持嵌套中断
 * 采用类似Linux的机制：禁用当前IRQ，开启全局中断允许其他IRQ嵌套
 */
void handle_interrupt_chain(int irq)
{
    struct interrupt_desc *desc = interrupt_table[irq];
    if (!desc)
        return;
    // 屏蔽当前IRQ源，保持关中断执行链上的处理函数，避免在内核态处理流程中被中断打断导致的重入/时序竞态
    disable_interrupt(irq);
    for (struct interrupt_desc *p = desc; p; p = p->next)
    {
        if (p->handler)
            p->handler();
    }
    enable_interrupt(irq);
}

/**
 * 设备中断处理
 * @return 中断号，0表示未识别的中断
 */
int devintr(void)
{
    uint64 scause = r_scause();

    // 检查是否是中断（最高位为1）
    if (!(scause & 0x8000000000000000L))
        return 0; // 不是中断

    int irq = SCAUSE_TO_IRQ(scause);
    int hw_irq = 0;

    switch (irq)
    {
    case IRQ_TIMER:
        break;
    case IRQ_EXTERNAL:
        hw_irq = plic_claim();
        if (hw_irq == 0)
            return 0;
        break;
    case IRQ_SOFTWARE:
        // 清除软件中断标志
        w_sip(r_sip() & ~(1L << 1));
        break;
    default:
        printf("Unknown interrupt: irq=%d, scause=0x%lx\n", irq, scause);
        return 0;
    }

    handle_interrupt_chain(irq);

    if (irq == IRQ_EXTERNAL)
        plic_complete(hw_irq);

    return irq;
}

static void handle_user_exception(struct proc *p, uint64 cause, uint64 epc, uint64 tval)
{
    printf("usertrap: unexpected cause=0x%lx epc=0x%lx tval=0x%lx pid=%d\n", cause, epc, tval, p->pid);
    setkilled(p);
}

void usertrap(void)
{
    if ((r_sstatus() & SSTATUS_SPP) != 0)
        panic("usertrap: not from user mode");

    w_stvec((uint64)kernelvec);

    struct proc *p = myproc();
    p->trapframe->epc = r_sepc();

    uint64 scause = r_scause();
    uint64 tval = r_stval();

    int which_dev = 0;

    if (scause == CAUSE_USER_ECALL)
    {
        if (killed(p))
            exit(-1);

        p->trapframe->epc += 4;
        intr_on();
        syscall();
    }
    else if ((which_dev = devintr()) != 0)
    {
        // 设备中断已经处理
    }
    else
    {
        // 尝试 COW, 再尝试懒分配（sbrk lazy）
        int handled = -1;
        if (scause == CAUSE_STORE_PAGE_FAULT || scause == CAUSE_LOAD_PAGE_FAULT)
        {
            // 先尝试可选的懒分配（若启用）
#if ENABLE_LAZY_SBRK
            if (handle_lazy_alloc(p, tval) == 0)
                handled = 0;
#endif
            // 再尝试 COW：针对已有映射但标记为 COW 的页
            if (handled != 0 && handle_cow_fault(p, tval) == 0)
                handled = 0;
        }
        if (handled != 0)
            handle_user_exception(p, scause, p->trapframe->epc, tval);
    }

    if (killed(p))
        exit(-1);

    if (which_dev == IRQ_TIMER && sched_should_yield(p))
        yield();

    usertrapret();
}

void usertrapret(void)
{
    struct proc *p = myproc();

    intr_off();

    uint64 trampoline_uservec = TRAMPOLINE + (uint64)(uservec - trampoline);
    w_stvec(trampoline_uservec);

    p->trapframe->kernel_satp = MAKE_SATP(kernel_pagetable);
    p->trapframe->kernel_sp = p->kstack + PGSIZE;
    p->trapframe->kernel_trap = (uint64)usertrap;
    p->trapframe->kernel_hartid = r_tp();

    uint64 x = r_sstatus();
    x &= ~SSTATUS_SPP;
    x |= SSTATUS_SPIE;
    w_sstatus(x);

    w_sepc(p->trapframe->epc);

    uint64 satp = MAKE_SATP(p->pagetable);
    uint64 fn = TRAMPOLINE + (uint64)(userret - trampoline);
    ((void (*)(uint64, uint64))fn)(satp, (uint64)p->trapframe);
    panic("usertrapret: unreachable");
}

/**
 * 处理页故障异常 - 实际的内存管理
 */
void handle_page_fault(uint64 cause, uint64 epc, uint64 tval)
{
    printf("Page fault occurred:\n");
    printf("  Fault address: 0x%lx\n", tval);
    printf("  Instruction address: 0x%lx\n", epc);
    printf("  Cause: 0x%lx\n", cause);

    // 获取当前页表（假设我们有全局的内核页表）
    extern pagetable_t kernel_pagetable;

    // 检查是否是访问内核地址空间的页故障
    if (tval >= KERNBASE  && tval < 0x88000000)
    { 
        // 计算页对齐的地址
        uint64 fault_page = tval & ~(PGSIZE - 1);

        printf("  Attempting to map page at 0x%lx\n", fault_page);

        // 分配一个物理页
        void *pa = alloc_page();
        if (pa == 0)
        {
            panic("Out of memory during page fault handling");
        }

        printf("  Allocated physical page at 0x%lx\n", (uint64)pa);

        // 清零新分配的页
        memset(pa, 0, PGSIZE);

        // 映射到页表中（可读写权限）
        int result = mappages(kernel_pagetable, fault_page, PGSIZE, (uint64)pa, PTE_R | PTE_W);
        if (result != 0)
        {
            printf("  Failed to map page\n");
            free_page(pa); // 释放分配的物理页
            panic("Failed to map page during page fault handling");
        }

        printf("  Successfully mapped page 0x%lx -> 0x%lx\n", fault_page, (uint64)pa);
        printf("  Continuing execution...\n");

        return;
    }

    // 如果是访问用户地址空间或无效地址
    printf("  Invalid memory access to address 0x%lx\n", tval);

    if (cause == CAUSE_LOAD_PAGE_FAULT)
    {
        printf("  Type: Load page fault\n");
    }
    else if (cause == CAUSE_STORE_PAGE_FAULT)
    {
        printf("  Type: Store page fault\n");
    }
    else if (cause == CAUSE_FETCH_PAGE_FAULT)
    {
        printf("  Type: Instruction fetch page fault\n");
    }

    panic("Unhandled page fault");
}

/**
 * 异常处理函数
 */
void handle_exception(uint64 cause, uint64 epc, uint64 tval)
{
    printf("Exception: cause=0x%lx, epc=0x%lx, tval=0x%lx, nested=%d\n", cause, epc, tval, nested_level);

    switch (cause)
    {
    case CAUSE_USER_ECALL:
        printf("  Type: User environment call (syscall)\n");
        // 这里可以调用系统调用处理函数
        printf("  Syscall handling not implemented yet\n");
        break;
    case CAUSE_SUPERVISOR_ECALL:
        printf("  Type: Supervisor environment call\n");
        break;
    case CAUSE_MACHINE_ECALL:
        printf("  Type: Machine environment call\n");
        break;
    case CAUSE_ILLEGAL_INSTRUCTION:
        printf("  Type: Illegal instruction\n");
        printf("  Illegal instruction at 0x%lx\n", epc);
        break;
    case CAUSE_BREAKPOINT:
        printf("  Type: Breakpoint\n");
        printf("  Breakpoint at 0x%lx\n", epc);
        // 对于调试，可以选择继续或panic
        break;
    case CAUSE_FETCH_PAGE_FAULT:
        printf("  Type: Instruction page fault\n");
        handle_page_fault(cause, epc, tval);
        return; // 页故障处理可能会恢复执行
    case CAUSE_LOAD_PAGE_FAULT:
        printf("  Type: Load page fault\n");
        handle_page_fault(cause, epc, tval);
        return; // 页故障处理可能会恢复执行
    case CAUSE_STORE_PAGE_FAULT:
        printf("  Type: Store page fault\n");
        handle_page_fault(cause, epc, tval);
        return; // 页故障处理可能会恢复执行
    default:
        printf("  Type: Unknown exception (cause=0x%lx)\n", cause);
        break;
    }

    panic("Unhandled exception");
}

/**
 * 获取当前时间
 */
uint64 get_time(void)
{
    return r_time();
}

/**
 * 设置下次时钟中断时间
 */
void set_next_timer(uint64 interval)
{
    w_stimecmp(r_time() + interval);
}

static int handle_cow_fault(struct proc *p, uint64 va)
{
    uint64 fault_page = PGROUNDDOWN(va);
    //printf("trap: pid %d COW fault va=0x%lx\n", p->pid, fault_page);
    if (cow_allocpage(p->pagetable, fault_page) == 0)
    {
        //printf("trap: pid %d COW handled va=0x%lx\n", p->pid, fault_page);
        return 0;
    }
    printf("trap: pid %d COW failed va=0x%lx\n", p->pid, fault_page);
    setkilled(p);
    return -1;
}

// 懒分配：对落在 [0, p->sz) 且尚未映射的用户页，分配并映射零页
#if ENABLE_LAZY_SBRK
static int handle_lazy_alloc(struct proc *p, uint64 va)
{
    uint64 fault = PGROUNDDOWN(va);
    if (fault >= p->sz)
        return -1; // 超出进程大小

    // 检查是否已存在有效PTE（包括内核不可达的守护页等）
    // 注意：walkaddr 仅在 PTE_U 置位时返回物理地址；
    // 守护页通过清除 PTE_U 来禁止用户访问，但其 PTE 仍是有效的(PTE_V=1)。
    // 若使用 walkaddr 判断，则会误认为未映射并尝试 remap，导致 mappages: remap。
    // 因此这里直接查看底层 PTE 是否有效；若有效则不进行懒分配，由上层按异常处理。
    pagetable_t pt = p->pagetable;
    if (pte_is_valid(pt, fault))
        return -1; // 已有有效PTE（可能是守护页），不应在此分配

    extern void *alloc_page(void);
    extern int mappages(pagetable_t, uint64, uint64, uint64, int);

    char *mem = alloc_page();
    if (mem == 0)
        return -1;
    memset(mem, 0, PGSIZE);
    if (mappages(pt, fault, PGSIZE, (uint64)mem, PTE_U | PTE_R | PTE_W) < 0) {
        free_page(mem);
        return -1;
    }
    return 0;
}
#endif