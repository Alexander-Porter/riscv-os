#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "proc.h"
#include "global_func.h"

// PLIC 初始化：为需要的外设设置优先级
void plic_init(void)
{
    // 将 UART 与 virtio 磁盘的优先级设为非零，允许它们触发中断
    *(volatile uint32 *)(PLIC_PRIORITY + UART0_IRQ * sizeof(uint32)) = 1;
    *(volatile uint32 *)(PLIC_PRIORITY + VIRTIO0_IRQ * sizeof(uint32)) = 1;
}

// 为当前 hart 启用 PLIC 中断，并设置阈值
void plic_init_hart(void)
{
    int hart = cpuid();
    // 允许当前 hart 在 S 模式接收来自 UART 与 virtio 的中断
    *(volatile uint32 *)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);
    // 设置中断优先级阈值为 0，表示接受所有优先级的中断
    *(volatile uint32 *)PLIC_SPRIORITY(hart) = 0;
}

// 读取 PLIC，获得当前待处理的外部中断号
int plic_claim(void)
{
    int hart = cpuid();
    return *(volatile uint32 *)PLIC_SCLAIM(hart);
}

// 通知 PLIC 已完成中断处理
void plic_complete(int irq)
{
    int hart = cpuid();
    *(volatile uint32 *)PLIC_SCLAIM(hart) = irq;
}
