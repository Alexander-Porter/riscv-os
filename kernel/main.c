#include "global_func.h"
#include "memlayout.h"
#include "paging.h"      // 引入 pagetable_t 与 dump_pagetable 原型
#include "types.h"
#include "sbi.h"
#include "paging.h"

// from start.c
extern char stack0[];

// 内核主函数
void main()
{
    clear_screen();
    printf("Hello, Gemini-OS!\n");

    printf("Initializing memory management...\n");
    pmm_init();         // 初始化物理内存管理器

    kvm_init();         // 创建内核页表
    kvm_init_hart();    // 启用分页
    printf("Paging enabled.\n");

    printf("Initializing process table...\n");
    proc_init();        // 初始化进程表
    user_init();        // 创建第一个用户进程

    printf("Initializing trap handling...\n");
    // 设置sscratch指向内核栈顶, 为中断处理做准备
    w_sscratch((uint64)stack0 + 4096);
    trapinithart();     // 初始化中断向量和使能
    printf("Trap handling initialized.\n");

    printf("Starting scheduler...\n");
    scheduler();
}
