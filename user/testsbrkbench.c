// 替换为并发文件创建/写入/校验用例：两个子进程各写一个小文件
#include "user.h"

static int write_text(const char *path, const char *s)
{
    int fd = open(path, O_CREATE | O_RDWR);
    if (fd < 0) return -1;
    int n = (int)strlen(s);
    int m = write(fd, s, n);
    close(fd);
    return m == n ? 0 : -1;
}

static int file_size(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return -1; }
    close(fd);
    return (int)st.size;
}

int main(void)
{
    const char *f0 = "c0";
    const char *f1 = "c1";

    int pid = fork();
    if (pid < 0) { printf("concfs: fork failed\n"); exit(-1); }
    if (pid == 0)
    {
        if (write_text(f0, "child0") < 0) exit(-1);
        exit(0);
    }

    int pid2 = fork();
    if (pid2 < 0) { printf("concfs: fork2 failed\n"); exit(-1); }
    if (pid2 == 0)
    {
        if (write_text(f1, "child1") < 0) exit(-1);
        exit(0);
    }

    int st;
    wait(&st);
    wait(&st);

    int s0 = file_size(f0);
    int s1 = file_size(f1);
    if (s0 <= 0 || s1 <= 0) { printf("concfs: size check failed s0=%d s1=%d\n", s0, s1); exit(-1); }

    unlink(f0);
    unlink(f1);
    printf("testfs_concurrent: PASS\n");
    exit(0);
}
