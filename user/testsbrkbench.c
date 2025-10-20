#include "user.h"

#define PAGE 4096
// 选择较保守的页数，确保在10s超时内完成并留出其他测试开销
#define N_PAGES 256

static inline uint64 tstart(void) { return rdtime(); }
static inline uint64 tend(void) { return rdtime(); }

// 基准A：逐页 sbrk，但不触碰内存（只测量系统调用与元数据开销）
static uint64 bench_sbrk_only(int pages)
{
    uint64 t0 = tstart();
    for (int i = 0; i < pages; i++) {
        if (sbrk(PAGE) == SBRK_ERROR) return 0; // 0 表示失败
    }
    return tend() - t0;
}

// 基准B：逐页 sbrk 后立刻触碰该页（模拟“分配立即使用”，触发每页缺页/分配）
static uint64 bench_interleaved_sbrk_touch(int pages)
{
    uint64 t0 = tstart();
    for (int i = 0; i < pages; i++) {
        char *p = sbrk(PAGE);
        if (p == SBRK_ERROR) return 0;
        p[0] = (char)i;
        p[PAGE/2] = (char)(i ^ 0x33);
    }
    return tend() - t0;
}

int main(void)
{
    printf("testsbrkbench: BEGIN (pages=%d)\n", N_PAGES);

    // 基准A：连续 sbrk（不触碰）
    uint64 cA = bench_sbrk_only(N_PAGES);
    if (!cA) { printf("testsbrkbench: bench_sbrk_only failed\n"); exit(-1); }
    printf("A) sbrk-only (pages=%d): %x cycles\n", N_PAGES, (unsigned)cA);

    // 基准B：sbrk 后立即触碰（每页）
    uint64 cB = bench_interleaved_sbrk_touch(N_PAGES);
    if (!cB) { printf("testsbrkbench: bench_interleaved_sbrk_touch failed\n"); exit(-1); }
    printf("B) sbrk+touch (pages=%d): %x cycles\n", N_PAGES, (unsigned)cB);

    printf("testsbrkbench: END\n");
    exit(0);
}
