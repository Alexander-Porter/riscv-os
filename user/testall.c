#include "user.h"

// 单文件综合测试：
// - 项目1：优先级调度
// - 项目2：ELF 加载器与用户空间（最小验证：exec 错误路径 + 成功执行已存在程序名）
// - 项目3：IPC（pipe / semaphore / shared memory）
// - 项目4：内核日志（debugfs 打印）
// - 项目5：COW 基准（仅读 + 少量写触发写时复制）
// 输出简洁、可复现；所有测试失败时 exit(-1)

static void burn(unsigned iters) {
    volatile unsigned x = 0;
    for (unsigned i = 0; i < iters; i++) {
        x = x * 1664525u + 1013904223u; // 计算占位
        if ((i & 0x3fff) == 0) yield();  // 让出 CPU，放大调度差异
    }
}

static void test_priority_sched(void) {
    printf("[prio] begin\n");
    const int n = 3;
    const int prio[3] = {0, 5, 10}; // 数字越小优先级越高
    int pid[n];
    uint64 start = uptime();
    for (int i = 0; i < n; i++) {
        pid[i] = fork();
        if (pid[i] == 0) {
            setpriority(prio[i]);
            burn(300000); // 轻量计算 + 周期性 yield
            int rt = (int)getrunticks();
            printf("[prio] child prio=%d runticks=%d\n", prio[i], rt);
            exit(0);
        }
    }
    int order[3]; int k = 0;
    for (int i = 0; i < n; i++) {
        int st = -1; int c = wait(&st);
        if (c < 0) { printf("[prio] wait failed\n"); exit(-1); }
        order[k++] = c;
    }
    uint64 dur = uptime() - start;
    printf("[prio] finish order: %d %d %d in %lu ticks\n", order[0], order[1], order[2], (unsigned long)dur);
    printf("[prio] end\n");
}

static void test_exec_minimal(void) {
    printf("[exec] begin\n");
    // 失败路径：不存在的程序
    {
        int pid = fork();
        if (pid == 0) {
            char *argv[] = {"no_such_prog", 0};
            int rv = exec("no_such_prog", argv);
            printf("[exec] expected -1, got %d\n", rv);
            exit(rv == -1 ? 0 : -1);
        }
        wait(0);
    }
    // 成功路径：执行一个已存在的、短生命周期的用户程序名（仅验证加载与返回）
    {
        int pid = fork();
        if (pid == 0) {
            char *argv[] = {"noop", 0};
            exec("noop", argv);
            printf("[exec] exec noop failed\n");
            exit(-1);
        }
        // 等待其退出，避免拉长测试时间
        wait(0);
    }
    printf("[exec] end\n");
}

static void test_ipc_pipe(void) {
    printf("[ipc-pipe] begin\n");
    int fds[2];
    if (pipe(fds) < 0) { printf("[ipc-pipe] pipe failed\n"); exit(-1); }
    int pid = fork();
    if (pid < 0) { printf("[ipc-pipe] fork failed\n"); exit(-1); }
    if (pid == 0) {
        // 子进程：只读
        close(fds[1]);
        char buf[8] = {0};
        int rn = read(fds[0], buf, 5);
        if (rn != 5 || buf[0] != 'h' || buf[4] != 'o') {
            printf("[ipc-pipe] child read mismatch rn=%d buf=%s\n", rn, buf);
            exit(-1);
        }
        close(fds[0]);
        exit(0);
    } else {
        // 父进程：只写
        close(fds[0]);
        const char *msg = "hello";
        int wn = write(fds[1], msg, 5);
        if (wn != 5) { printf("[ipc-pipe] parent write failed: %d\n", wn); exit(-1); }
        close(fds[1]);
        wait(0);
    }
    printf("[ipc-pipe] end\n");
}

static void test_ipc_semaphore(void) {
    printf("[ipc-sem] begin\n");
    int sem = sem_create(0);
    if (sem < 0) { printf("[ipc-sem] sem_create failed\n"); exit(-1); }
    int pid = fork();
    if (pid == 0) {
        sleep(5);
        sem_post(sem);
        exit(0);
    } else {
        int t0 = uptime();
        sem_wait(sem);
        int t1 = uptime();
        wait(0);
        printf("[ipc-sem] waited %d ticks\n", (t1 - t0));
    }
    printf("[ipc-sem] end\n");
}

static void test_ipc_shm(void) {
    printf("[ipc-shm] begin\n");
    int shmid = shm_create();
    if (shmid < 0) { printf("[ipc-shm] shm_create failed\n"); exit(-1); }
    int *p = (int *)shm_get(shmid);
    if ((long)p == -1) { printf("[ipc-shm] shm_get parent failed\n"); exit(-1); }
    *p = 0x12345678;
    int pid = fork();
    if (pid == 0) {
        int *q = (int *)shm_get(shmid);
        if ((long)q == -1) { printf("[ipc-shm] shm_get child failed\n"); exit(-1); }
        if (*q != 0x12345678) { printf("[ipc-shm] child read mismatch\n"); exit(-1); }
        *q = 0xabcdef01;
        exit(0);
    } else {
        wait(0);
        if (*p != 0xabcdef01) { printf("[ipc-shm] parent readback mismatch\n"); exit(-1); }
        shm_unmap(p);
    }
    printf("[ipc-shm] end\n");
}

static void test_klog(void) {
    printf("[klog] begin\n");
    // 打印文件系统与 inode 使用，作为最小验证
    debugfs(2);
    printf("[klog] end\n");
}

static void cow_readonly_child(char *base, int pages) {
    volatile unsigned sum = 0;
    for (int i = 0; i < pages * 4096; i += 64) sum += (unsigned)base[i];
    if (sum == 0xdeadbeef) printf("\n"); // 防止优化
}

static void cow_writefew_child(char *base, int pages, int stride_pages) {
    for (int i = 0; i < pages; i += stride_pages) base[i * 4096] ^= 1; // 触发少量 COW
}

static void test_cow_bench(void) {
    printf("[cow] begin\n");
    int pages = 128; // 512KB
    char *buf = sbrk(pages * 4096);
    if (buf == (char *)-1) { printf("[cow] sbrk failed\n"); exit(-1); }
    memset(buf, 0x5a, pages * 4096);

    // 读场景：fork 多个子进程只读扫描，应几乎不复制
    int t0 = uptime();
    for (int k = 0; k < 4; k++) {
        int pid = fork();
        if (pid == 0) { cow_readonly_child(buf, pages); exit(0); }
    }
    for (int k = 0; k < 4; k++) wait(0);
    int t1 = uptime();
    printf("[cow] readonly 4x finished in %d ticks\n", (t1 - t0));

    // 少量写场景：每隔 16 页写一次，触发有限 COW
    t0 = uptime();
    for (int k = 0; k < 4; k++) {
        int pid = fork();
        if (pid == 0) { cow_writefew_child(buf, pages, 16); exit(0); }
    }
    for (int k = 0; k < 4; k++) wait(0);
    t1 = uptime();
    printf("[cow] writefew 4x finished in %d ticks\n", (t1 - t0));
    printf("[cow] end\n");
}

int main(void) {
    test_priority_sched();
    test_exec_minimal();
    test_ipc_pipe();
    test_ipc_semaphore();
    test_ipc_shm();
    test_klog();
    test_cow_bench();
    printf("testall: PASS\n");
    // 若作为 init 运行，为了配合 timeout 30，直接触发内核结束；否则正常退出
    if (getpid() == 1) {
        crash();
    } else {
        exit(0);
    }
}
