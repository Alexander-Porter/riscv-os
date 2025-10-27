#include "user.h"

// 两阶段崩溃恢复测试：
// 1) 第一次运行：创建标记文件并写入内容 -> 立即触发内核崩溃
// 2) 第二次运行：检测标记文件内容是否正确 -> 删除标记 -> PASS

static const char *MARK = "RECOVERY_MARK";

static void first_run(void)
{
    // 先创建空文件并提交（避免在创建事务就触发崩溃）
    int fd = open(MARK, O_CREATE | O_RDWR);
    if (fd < 0) { printf("recover: create mark failed\n"); exit(-1); }
    close(fd);

    // 开启“提交后崩溃”模式：在写入数据的提交点（写完日志头）panic
    debugfs(10);

    fd = open(MARK, O_RDWR);
    if (fd < 0) { printf("recover: reopen mark failed\n"); exit(-1); }
    const char *msg = "journal-ok";
    int n = (int)strlen(msg);
    if (write(fd, msg, n) != n) { printf("recover: write failed\n"); exit(-1); }
    close(fd);
    // 正常情况下，上面的提交过程会在内核中 panic，不会到达此处
    printf("recover: WARN: forced crash did not trigger\n");
    crash();
}

static void second_run(void)
{
    // 确保关闭“提交后崩溃”模式（理论上重启后默认为关闭，这里显式清除）
    debugfs(11);
    int fd = open(MARK, O_RDONLY);
    if (fd < 0) { printf("recover: second_run cannot open mark\n"); exit(-1); }
    char buf[32] = {0};
    int r = read(fd, buf, sizeof(buf));
    close(fd);
    if (r <= 0 || strncmp(buf, "journal-ok", 10) != 0) {
        printf("recover: content mismatch\n");
        exit(-1);
    }
    unlink(MARK);
    printf("testfs_recover: PASS\n");
    // 若作为 init(pid==1) 启动，保持循环以避免内核 panic；否则正常退出
    if (getpid() == 1) {
        for (;;)
            sleep(1000);
    }
    exit(0);
}

int main(void)
{
    int fd = open(MARK, O_RDONLY);
    if (fd < 0) {
        // 未找到标记 -> 第一次运行
        first_run();
    } else {
        close(fd);
        second_run();
    }
    return 0;
}
