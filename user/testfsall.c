#include "user.h"

// 统一文件系统测试：
// - 基本文件/目录操作正确性
// - 并发访问稳定性
// - 简单性能评测（小文件批量 + 大文件顺序写）
// 输出尽量保持简洁、可比对；必要处打印 PASS/FAIL

static int str_eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static void utoa10(unsigned v, char *buf) {
    char tmp[16]; int n = 0;
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    for (int i = 0; i < n; i++) buf[i] = tmp[n-1-i];
    buf[n] = 0;
}

// ===== 基本文件/目录测试 =====
static void test_basic(void) {
    printf("=== FS basic tests ===\n");

    // 1) 创建/写入/读取/覆盖
    {
        int fd = open("basic.txt", O_CREATE | O_RDWR);
        if (fd < 0) { printf("basic: open create failed\n"); exit(-1); }
        const char *msg = "hello";
        if (write(fd, msg, 5) != 5) { printf("basic: write failed\n"); exit(-1); }
        close(fd);

        fd = open("basic.txt", O_RDONLY);
        if (fd < 0) { printf("basic: reopen failed\n"); exit(-1); }
        char buf[16] = {0};
        int r = read(fd, buf, sizeof(buf));
        close(fd);
        if (r != 5 || !str_eq(buf, "hello")) { printf("basic: read content mismatch\n"); exit(-1); }

        // 覆盖写（截断）
        fd = open("basic.txt", O_RDWR | O_TRUNC);
        if (fd < 0) { printf("basic: open trunc failed\n"); exit(-1); }
        const char *msg2 = "world";
        if (write(fd, msg2, 5) != 5) { printf("basic: write world failed\n"); exit(-1); }
        close(fd);

        fd = open("basic.txt", O_RDONLY);
        if (fd < 0) { printf("basic: reopen2 failed\n"); exit(-1); }
        for (int i = 0; i < (int)sizeof(buf); i++) buf[i] = 0;
        r = read(fd, buf, sizeof(buf));
        close(fd);
        if (r != 5 || !str_eq(buf, "world")) { printf("basic: read world mismatch\n"); exit(-1); }

        if (unlink("basic.txt") < 0) { printf("basic: unlink basic.txt failed\n"); exit(-1); }
    }

    // 2) 目录/路径解析与创建、删除
    {
        if (mkdir("dir1") < 0) { printf("basic: mkdir dir1 failed\n"); exit(-1); }
        int fd = open("dir1/a", O_CREATE | O_RDWR);
        if (fd < 0) { printf("basic: create dir1/a failed\n"); exit(-1); }
        int val = 1234;
        if (write(fd, &val, sizeof(val)) != (int)sizeof(val)) { printf("basic: write dir1/a failed\n"); exit(-1); }
        close(fd);

        fd = open("dir1/a", O_RDONLY);
        if (fd < 0) { printf("basic: open dir1/a failed\n"); exit(-1); }
        int got = 0; int r = read(fd, &got, sizeof(got));
        close(fd);
        if (r != (int)sizeof(got) || got != val) { printf("basic: read dir1/a mismatch\n"); exit(-1); }

        if (unlink("dir1/a") < 0) { printf("basic: unlink dir1/a failed\n"); exit(-1); }
        // xv6风格：空目录可直接 unlink 删除
        if (unlink("dir1") < 0) { printf("basic: unlink dir1 failed\n"); exit(-1); }
    }

    // 3) 硬链接
    {
        int fd = open("foo", O_CREATE | O_RDWR);
        if (fd < 0) { printf("basic: create foo failed\n"); exit(-1); }
        const char *s = "data";
        if (write(fd, s, 4) != 4) { printf("basic: write foo failed\n"); exit(-1); }
        close(fd);

        if (link("foo", "bar") < 0) { printf("basic: link foo->bar failed\n"); exit(-1); }
        if (unlink("foo") < 0) { printf("basic: unlink foo failed\n"); exit(-1); }

        fd = open("bar", O_RDONLY);
        if (fd < 0) { printf("basic: open bar failed\n"); exit(-1); }
        char b[8] = {0}; int r = read(fd, b, sizeof(b));
        close(fd);
        if (r != 4 || strncmp(b, "data", 4) != 0) { printf("basic: bar content mismatch\n"); exit(-1); }
        if (unlink("bar") < 0) { printf("basic: unlink bar failed\n"); exit(-1); }
    }

    printf("basic: PASS\n");
}

// ===== 并发访问测试 =====
static void test_concurrent_access(void) {
    printf("Testing concurrent file access…\n");
    const int nproc = 4;
    const int iterations = 30; // 降低迭代次数以在30秒内完成整体测试
    for (int i = 0; i < nproc; i++) {
        int pid = fork();
        if (pid == 0) {
            // 子进程：对各自文件反复 create/write/close/unlink
            char name[32];
            strcpy(name, "test_"); int k = (int)strlen(name);
            if (i < 10) { name[k++] = '0' + i; name[k] = 0; }
            else {
                char num[16]; utoa10((unsigned)i, num);
                int j = 0; while (num[j] && k < (int)sizeof(name)-1) name[k++] = num[j++];
                name[k] = 0;
            }
            for (int t = 0; t < iterations; t++) {
                int fd = open(name, O_CREATE | O_RDWR);
                if (fd >= 0) {
                    if (write(fd, &t, sizeof(t)) != (int)sizeof(t)) {
                        printf("concur: write failed\n"); exit(-1);
                    }
                    close(fd);
                    unlink(name);
                }
            }
            exit(0);
        }
    }
    for (int i = 0; i < nproc; i++)
        wait(0);
    printf("Concurrent access test completed\n");
}

// ===== 简单性能测试 =====
static void small_files_bench(void) {
    const int files = 16;
    const char *name = "smf";
    // 与 xv6 对齐：使用 uptime() 的“tick”为计时单位
    uint64 t0 = uptime();
    for (int i = 0; i < files; i++) {
        char fn[32]; char num[16];
        strcpy(fn, name); int k = (int)strlen(fn);
        fn[k++] = '_'; fn[k] = 0;
        utoa10((unsigned)i, num);
        int j = 0; while (num[j] && k < (int)sizeof(fn)-1) { fn[k++] = num[j++]; }
        fn[k] = 0;
        int fd = open(fn, O_CREATE | O_RDWR);
        if (fd < 0) { printf("perf: open %s failed\n", fn); exit(-1); }
        int v = i;
        if (write(fd, &v, sizeof(v)) != (int)sizeof(v)) { printf("perf: write failed\n"); exit(-1); }
        close(fd);
    }
    uint64 t1 = uptime();
    printf("perf_small_files: %d files in %lu ticks\n", files, (unsigned long)(t1 - t0));
    // cleanup
    for (int i = 0; i < files; i++) {
        char fn[32]; char num[16];
        strcpy(fn, name); int k = (int)strlen(fn);
        fn[k++] = '_'; fn[k] = 0;
        utoa10((unsigned)i, num);
        int j = 0; while (num[j] && k < (int)sizeof(fn)-1) { fn[k++] = num[j++]; }
        fn[k] = 0;
        unlink(fn);
    }
}

static void large_file_bench(void) {
    // 与 xv6 对齐：顺序写 256KB，按 1KB 块写入 256 次
    const int kb = 256;
    char buf[1024];
    printf("Preparing %dKB large file write benchmark...\n", kb);
    memset(buf, 0x5a, sizeof(buf));
    printf("Starting large file write benchmark...\n");
    uint64 t0 = uptime();
    int fd = open("large_file", O_CREATE | O_RDWR);
    if (fd < 0) { printf("perf: open large_file failed\n"); exit(-1); }
    for (int i = 0; i < kb; i++) {
        if (write(fd, buf, sizeof(buf)) != (int)sizeof(buf)) { printf("perf: write large failed\n"); exit(-1); }
    }
    printf("Large file write benchmark completed, closing file...\n");
    close(fd);
    uint64 t1 = uptime();
    printf("perf_large_file: %dKB in %lu ticks\n", kb, (unsigned long)(t1 - t0));
    unlink("large_file");
}

static void test_perf(void) {
    small_files_bench();
    large_file_bench();
    printf("testfs_perf: DONE\n");
}

int main(void) {
    test_basic();
    // 将性能测试提前，以确保在30秒超时窗口内输出性能结果
    test_perf();
    test_concurrent_access();
    printf("testfs_all: PASS\n");
    // —— 崩溃恢复测试：第一阶段 ——
    // 正确顺序应为：
    //  1) 预先创建空的标记文件（不在崩溃模式下），避免在“创建事务”的提交点就崩溃
    //  2) 开启“提交后崩溃”模式
    //  3) 重新打开并写入内容，触发一次写事务，在写完日志头后 panic
    int fd = open("RECOVERY_MARK", O_CREATE | O_RDWR);
    if (fd < 0) { printf("crash-stage1: create mark failed\n"); exit(-1); }
    close(fd);

    // 现在开启“提交后崩溃”模式：在写入数据的提交点（写完日志头）panic
    debugfs(10);

    fd = open("RECOVERY_MARK", O_RDWR);
    if (fd < 0) { printf("crash-stage1: reopen mark failed\n"); exit(-1); }
    const char *msg = "journal-ok";
    int n = (int)strlen(msg);
    if (write(fd, msg, n) != n) { printf("crash-stage1: write failed\n"); exit(-1); }
    close(fd);
    // 正常情况下，上面的提交过程会在内核中 panic，不会到达此处
    printf("crash-stage1: WARN: forced crash did not trigger\n");
    crash();
    // 不可达
    return 0;
}
