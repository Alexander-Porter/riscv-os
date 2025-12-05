// COW Fork-Exec 性能基准：数千次 fork 之后立即 exec
// 用于和 xv6 的无 COW 实现对比
#include "user.h"

#define PGSIZE 4096
#define DEFAULT_ITERS 2000        // 默认循环次数（保持在超时限制内）
#define DEFAULT_PARENT_PAGES 32   // 父进程触摸的页数，制造可见的内存复制开销

static int parse_int(const char *s, int def)
{
    // 简单的十进制解析，非法输入返回默认值
    if (s == 0 || *s == 0)
        return def;
    int v = 0;
    for (const char *p = s; *p; p++)
    {
        if (*p < '0' || *p > '9')
            return def;
        v = v * 10 + (*p - '0');
    }
    return v;
}

// 触摸父进程的内存，确保页表映射生效
static void touch_parent_pages(char *base, int pages)
{
    for (int i = 0; i < pages; i++)
    {
        base[i * PGSIZE] = (char)(i & 0xff);
    }
}

// 实际的 fork-exec 基准循环：每次 fork 后立刻 exec 一个极轻量程序
static void run_bench(int iters, int parent_pages)
{
    printf("[bench] iters=%d parent_pages=%d\n", iters, parent_pages);

    // 父进程分配并触摸内存，便于观察 COW 与全量复制的差异
    int total_bytes = parent_pages * PGSIZE;
    char *mem = sbrk(total_bytes);
    if (mem == (char *)-1)
    {
        printf("[bench] sbrk failed\n");
        exit(-1);
    }
    touch_parent_pages(mem, parent_pages);

    uint64 t0 = uptime();
    int ok = 0, fail = 0;

    for (int i = 0; i < iters; i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            fail++;
            // 回收可能残留的子进程，避免表满
            wait(0);
            continue;
        }

        if (pid == 0)
        {
            // 子进程立刻 exec 极小负载程序
            char *argv_child[] = {"forkexec_child", 0};
            exec("forkexec_child", argv_child);
            printf("[bench] child exec failed\n");
            exit(-1);
        }

        int status = 0;
        if (wait(&status) < 0)
        {
            printf("[bench] wait failed at %d\n", i);
            break;
        }
        if (status == 0)
            ok++;
        else
            fail++;
    }

    uint64 t1 = uptime();
    uint64 dt = t1 - t0;

    printf("[bench] total ticks: %lu\n", dt);
    printf("[bench] success=%d fail=%d\n", ok, fail);
    printf("[bench] avg ticks/op: %lu\n", ok ? dt / ok : dt);
}

int main(int argc, char **argv)
{
    int iters = (argc > 1) ? parse_int(argv[1], DEFAULT_ITERS) : DEFAULT_ITERS;
    int parent_pages = (argc > 2) ? parse_int(argv[2], DEFAULT_PARENT_PAGES) : DEFAULT_PARENT_PAGES;
    if (iters < 1)
        iters = DEFAULT_ITERS;
    if (parent_pages < 1)
        parent_pages = DEFAULT_PARENT_PAGES;

    printf("=== COW Fork-Exec Benchmark ===\n");
    printf("使用 uptime(ticks) 计时，便于与 xv6 对比\n");

    run_bench(iters, parent_pages);

    printf("=== Benchmark Done ===\n");
    if (getpid() == 1)
    {
        // 作为 init 运行时防止 QEMU 立即退出
        for (;;)
            sleep(1000);
    }
    exit(0);
}
