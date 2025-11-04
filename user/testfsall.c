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

static int stat_path(const char *path, struct stat *st) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    int r = fstat(fd, st);
    close(fd);
    return r;
}

static const char *type_name(short type) {
    switch (type) {
    case T_DIR:
        return "DIR";
    case T_FILE:
        return "FILE";
    case T_DEVICE:
        return "DEVICE";
    case T_SYMLINK:
        return "SYMLINK";
    default:
        return "UNKNOWN";
    }
}

static void show_link_info(const char *label, const char *path) {
    char buf[128];
    int len = readlink(path, buf, sizeof(buf) - 1);
    if (len >= 0) {
        if (len >= (int)sizeof(buf))
            len = sizeof(buf) - 1;
        buf[len] = '\0';
        struct stat st;
        if (stat_path(path, &st) == 0)
            printf("  %s: SYMLINK -> %s (target=%s nlink=%d)\n", label, buf, type_name(st.type), st.nlink);
        else
            printf("  %s: SYMLINK -> %s (target missing)\n", label, buf);
        return;
    }

    struct stat st;
    if (stat_path(path, &st) == 0)
        printf("  %s: %s nlink=%d size=%lu\n", label, type_name(st.type), st.nlink, (unsigned long)st.size);
    else
        printf("  %s: <missing>\n", label);
}

static void demo_link_semantics(void) {
    const char *base = "link_demo.txt";
    const char *hard = "link_demo.hard";
    const char *soft = "link_demo.soft";
    const char *payload = "link-demo-data";

    unlink(base);
    unlink(hard);
    unlink(soft);

    int fd = open(base, O_CREATE | O_RDWR);
    if (fd < 0) { printf("link-demo: create base failed\n"); exit(-1); }
    if (write(fd, payload, (int)strlen(payload)) != (int)strlen(payload)) { printf("link-demo: write base failed\n"); exit(-1); }
    close(fd);

    if (link(base, hard) < 0) { printf("link-demo: create hard link failed\n"); exit(-1); }
    if (symlink(base, soft) < 0) { printf("link-demo: create symlink failed\n"); exit(-1); }

    printf("link-demo: initial state\n");
    show_link_info("base", base);
    show_link_info("hard", hard);
    show_link_info("soft", soft);

    printf("link-demo: unlink hard link %s\n", hard);
    if (unlink(hard) < 0) { printf("link-demo: unlink hard failed\n"); exit(-1); }
    struct stat info;
    if (stat_path(base, &info) < 0) { printf("link-demo: stat base failed after hard unlink\n"); exit(-1); }
    printf("link-demo: base retained with nlink=%d\n", info.nlink);
    show_link_info("base", base);
    show_link_info("soft", soft);

    if (link(base, hard) < 0) { printf("link-demo: recreate hard link failed\n"); exit(-1); }

    printf("link-demo: unlink symlink %s\n", soft);
    if (unlink(soft) < 0) { printf("link-demo: unlink symlink failed\n"); exit(-1); }
    if (stat_path(base, &info) < 0) { printf("link-demo: stat base failed after symlink unlink\n"); exit(-1); }
    printf("link-demo: base unaffected by symlink removal, nlink=%d\n", info.nlink);
    show_link_info("base", base);
    show_link_info("hard", hard);

    if (symlink(base, soft) < 0) { printf("link-demo: recreate symlink failed\n"); exit(-1); }

    printf("link-demo: unlink original file %s\n", base);
    if (unlink(base) < 0) { printf("link-demo: unlink base failed\n"); exit(-1); }
    show_link_info("hard", hard);
    show_link_info("soft", soft);

    fd = open(hard, O_RDONLY);
    if (fd < 0) { printf("link-demo: hard link missing after base removal\n"); exit(-1); }
    char buf[32] = {0};
    int r = read(fd, buf, sizeof(buf));
    close(fd);
    if (r != (int)strlen(payload) || strncmp(buf, payload, strlen(payload)) != 0) {
        printf("link-demo: hard link data mismatch\n");
        exit(-1);
    }
    //printf("link-demo: hard link still provides data \"%s\"\n", payload);

    fd = open(soft, O_RDONLY);
    if (fd >= 0) {
        //printf("link-demo: symlink unexpectedly resolved after target removal\n");
        close(fd);
        exit(-1);
    } else {
        printf("link-demo: symlink open failed as expected after target removal\n");
    }

    char targetbuf[128];
    int linklen = readlink(soft, targetbuf, sizeof(targetbuf) - 1);
    if (linklen >= 0) {
        targetbuf[linklen] = '\0';
        //printf("link-demo: dangling symlink remembers target \"%s\"\n", targetbuf);
    } else {
        printf("link-demo: readlink failed unexpectedly\n");
        exit(-1);
    }

    if (unlink(hard) < 0) { printf("link-demo: cleanup hard failed\n"); exit(-1); }
    if (unlink(soft) < 0) { printf("link-demo: cleanup symlink failed\n"); exit(-1); }
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

static void test_symlink(void) {
    printf("=== FS symlink tests ===\n");

    int fd = open("sym_target", O_CREATE | O_RDWR);
    if (fd < 0) { printf("symlink: create target failed\n"); exit(-1); }
    const char *msg = "symlinks";
    if (write(fd, msg, 8) != 8) { printf("symlink: write target failed\n"); exit(-1); }
    close(fd);

    if (symlink("sym_target", "sym_link") < 0) { printf("symlink: create link failed\n"); exit(-1); }
    if (symlink("sym_link", "sym_chain") < 0) { printf("symlink: chain link failed\n"); exit(-1); }

    fd = open("sym_link", O_RDONLY);
    if (fd < 0) { printf("symlink: open link failed\n"); exit(-1); }
    char buf[64] = {0};
    int r = read(fd, buf, sizeof(buf));
    close(fd);
    if (r != 8 || strncmp(buf, msg, 8) != 0) { printf("symlink: read link mismatch\n"); exit(-1); }

    fd = open("sym_chain", O_RDONLY);
    if (fd < 0) { printf("symlink: open chain failed\n"); exit(-1); }
    for (int i = 0; i < (int)sizeof(buf); i++) buf[i] = 0;
    r = read(fd, buf, sizeof(buf));
    close(fd);
    if (r != 8 || strncmp(buf, msg, 8) != 0) { printf("symlink: chain read mismatch\n"); exit(-1); }

    char target[64];
    int n = readlink("sym_link", target, sizeof(target));
    if (n != (int)strlen("sym_target") || strncmp(target, "sym_target", n) != 0) {
        printf("symlink: readlink mismatch\n");
        exit(-1);
    }

    if (mkdir("symdir") < 0) { printf("symlink: mkdir symdir failed\n"); exit(-1); }
    if (symlink("../sym_target", "symdir/rel") < 0) { printf("symlink: relative link failed\n"); exit(-1); }
    fd = open("symdir/rel", O_RDONLY);
    if (fd < 0) { printf("symlink: open relative failed\n"); exit(-1); }
    for (int i = 0; i < (int)sizeof(buf); i++) buf[i] = 0;
    r = read(fd, buf, sizeof(buf));
    close(fd);
    if (r != 8 || strncmp(buf, msg, 8) != 0) { printf("symlink: relative read mismatch\n"); exit(-1); }

    if (unlink("sym_chain") < 0) { printf("symlink: unlink chain failed\n"); exit(-1); }
    if (unlink("sym_link") < 0) { printf("symlink: unlink link failed\n"); exit(-1); }
    if (unlink("symdir/rel") < 0) { printf("symlink: unlink rel failed\n"); exit(-1); }
    if (unlink("symdir") < 0) { printf("symlink: unlink symdir failed\n"); exit(-1); }
    if (unlink("sym_target") < 0) { printf("symlink: cleanup target failed\n"); exit(-1); }

    printf("=== Link semantics demo ===\n");
    demo_link_semantics();
    printf("symlink: PASS\n");
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
    test_symlink();
    debugfs(2);
    // 将性能测试提前，以确保在30秒超时窗口内输出性能结果
    test_perf();
    test_concurrent_access();
    debugfs(2);
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
