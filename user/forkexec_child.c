#include "user.h"

// forkexec_child：被基准测试进程 exec 的极轻量程序
#define PGSIZE 4096
// 仅做少量内存读写与计算，避免掩盖 fork 的开销
int main(int argc, char **argv)
{
    const int pages = 4; // 16KB 小工作集
    char *mem = sbrk(pages * PGSIZE);
    if (mem == (char *)-1)
        exit(-1);

    for (int i = 0; i < pages; i++)
    {
        mem[i * PGSIZE] = (char)(i + 1);
    }

    int acc = 0;
    for (int i = 0; i < 1000; i++)
        acc += (i & 7);

    return acc == 0x3E8 ? 0 : 0; // 常量折叠，不影响逻辑
}
