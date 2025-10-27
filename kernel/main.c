#include "global_func.h"
#include "memlayout.h"
#include "paging.h"      // 引入 pagetable_t 与 dump_pagetable 原型
#include "include/trap.h"
#include "include/riscv.h"
#include "proc.h"
#include "types.h"
#include "semaphore.h"
#include "shm.h"
#include "buf.h"
#include "virtio.h"
#include "fs.h"
#include "file.h"

// 内核主函数
void main()
{
    console_init();
    printf("Hello OS - Lab 5: Process Management\n");

    pmm_init();         // 初始化物理内存管理器
    kvm_init();         // 创建内核页表并映射内核栈
    kvm_init_hart();    // 启用分页

    procinit();         // 初始化进程表
    trap_init();        // 初始化中断/异常子系统
    trap_init_hart();   // 安装监督态陷阱向量
    plic_init();        // 初始化平台级中断控制器
    plic_init_hart();   // 启用当前 hart 的外部中断
    timer_init();       // 初始化时钟中断

    virtio_disk_init(); // 初始化 virtio 磁盘设备
    binit();            // 初始化块缓存
    iinit();            // 初始化 inode 表
    fileinit();         // 初始化文件表

    semaphore_system_init(); // 初始化内核同步原语
    shm_system_init();       // 初始化共享内存管理

    enable_interrupt(IRQ_TIMER);
    enable_interrupt(IRQ_EXTERNAL);
    intr_on();          // 允许外部与时钟中断在初始化阶段工作

    readsb(ROOTDEV, &sb);
    log_init(ROOTDEV, &sb); // 初始化日志子系统

    userinit();         // 创建第一个用户进程

    scheduler();        // 进入调度循环，不会返回
}
