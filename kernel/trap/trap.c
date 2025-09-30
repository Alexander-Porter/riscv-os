#include "../include/trap.h"
#include "../include/riscv.h"

#include "../global_func.h"
#include "../types.h"
#include "../paging.h"
#include "../memlayout.h"

// 声明外部函数
extern int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
extern void *alloc_page(void);
extern void free_page(void *pa);
extern void *memset(void *dst, int c, uint size);

// 全局变量
volatile uint64 ticks = 0;
struct interrupt_desc *interrupt_table[MAX_IRQ_NUM];
static int nested_level = 0;                        // 嵌套中断层级
static int current_priority = IRQ_PRIORITY_LOW + 1; // 当前处理的中断优先级


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

    // 增加嵌套层级
    nested_level++;

    // 处理中断或异常
    int which_dev = devintr();
    if (which_dev == 0)
    {
        // 异常处理
        handle_exception(scause, sepc, r_stval());
    }

    // 减少嵌套层级
    nested_level--;

    // 恢复寄存器
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

    // 如果没有注册的处理函数，直接返回
    if (!desc)
    {
        return;
    }

    // 获取当前IRQ线的优先级
    int irq_priority = (irq < MAX_IRQ_NUM) ? irq_priorities[irq] : IRQ_PRIORITY_LOW;

    // 检查是否允许嵌套中断（只有更高优先级的IRQ才能嵌套）
    if (irq_priority >= current_priority)
    {
        // 优先级不够高，不允许嵌套，直接返回
        printf("IRQ %d blocked (priority %d >= current %d)\n", irq, irq_priority, current_priority);
        return;
    }

    // 保存当前优先级
    int old_priority = current_priority;
    current_priority = irq_priority;



    // 类似Linux：禁用当前IRQ线，但开启全局中断允许其他IRQ嵌套
    disable_interrupt(irq); // 防止相同IRQ嵌套
    intr_on();              // 允许其他更高优先级IRQ嵌套

    // 执行该中断号上注册的所有处理函数（共享中断）
    desc = interrupt_table[irq];
    while (desc)
    {
        if (desc->handler)
        {
            desc->handler();
        }
        desc = desc->next;
    }

    // 恢复中断状态和优先级
    //intr_off();
    enable_interrupt(irq); // 重新启用当前IRQ线
    current_priority = old_priority;
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
    {
        return 0; // 不是中断
    }

    // 提取中断号
    int irq = SCAUSE_TO_IRQ(scause);

    // 根据中断类型进行特殊处理
    int skip_default_handle = 0;
    switch (irq)
    {
    case IRQ_TIMER:
        // 时钟中断，无需特殊处理
        break;
    case IRQ_EXTERNAL:
        break;
    case IRQ_SOFTWARE:
        // 清除软件中断标志
        w_sip(r_sip() & ~(1L << 1));
        break;
    default:
        printf("Unknown interrupt: irq=%d, scause=0x%lx\n", irq, scause);
        return 0;
    }

    // 统一处理所有中断，优先级由注册的处理函数决定
    if (!skip_default_handle) {
        handle_interrupt_chain(irq);
    }
    return irq;
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