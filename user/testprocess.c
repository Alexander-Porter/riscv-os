// 替换为文件系统基础用例：创建/写入/读取/stat/删除、目录创建与切换
#include "user.h"

static int write_full(int fd, const void *buf, int n)
{
    const char *p = (const char *)buf;
    int left = n;
    while (left > 0)
    {
        int m = write(fd, p, left);
        if (m <= 0) return -1;
        left -= m;
        p += m;
    }
    return n;
}

static int read_full(int fd, void *buf, int n)
{
    char *p = (char *)buf;
    int got = 0;
    while (got < n)
    {
        int m = read(fd, p + got, n - got);
        if (m < 0) return -1;
        if (m == 0) break;
        got += m;
    }
    return got;
}

static int test_fs_basic(void)
{
    const char *name = "hello";
    const char *data = "abcdefg012345";
    int len = (int)strlen(data);

    int fd = open(name, O_CREATE | O_RDWR);
    if (fd < 0) { printf("fs_basic: open create failed\n"); return -1; }
    if (write_full(fd, data, len) != len) { printf("fs_basic: write failed\n"); return -1; }
    if (close(fd) < 0) { printf("fs_basic: close failed\n"); return -1; }

    fd = open(name, O_RDONLY);
    if (fd < 0) { printf("fs_basic: reopen failed\n"); return -1; }
    char buf[32];
    int r = read_full(fd, buf, len);
    if (r != len || strncmp(buf, data, len) != 0) {
        printf("fs_basic: read/verify failed r=%d\n", r);
        printf("got: ");
        for (int i = 0; i < r; i++) printf("%x ", (unsigned char)buf[i]);
        printf("\nexp: ");
        for (int i = 0; i < len; i++) printf("%x ", (unsigned char)data[i]);
        printf("\n");
        return -1;
    }
    struct stat st;
    if (fstat(fd, &st) < 0 || st.type != T_FILE || st.size != (uint)len) { printf("fs_basic: fstat failed\n"); return -1; }
    close(fd);

    if (unlink(name) < 0) { printf("fs_basic: unlink failed\n"); return -1; }

    // 目录创建与切换
    if (mkdir("d") < 0) { printf("fs_basic: mkdir failed\n"); return -1; }
    if (chdir("d") < 0) { printf("fs_basic: chdir d failed\n"); return -1; }
    fd = open("inner", O_CREATE | O_RDWR);
    if (fd < 0) { printf("fs_basic: create inner failed\n"); return -1; }
    write_full(fd, "x", 1);
    close(fd);
    if (chdir("..") < 0) { printf("fs_basic: chdir .. failed\n"); return -1; }
    if (unlink("d/inner") < 0) { printf("fs_basic: unlink d/inner failed\n"); return -1; }
    if (unlink("d") < 0) { printf("fs_basic: rmdir d failed\n"); return -1; }

    printf("testfs_basic: PASS\n");
    return 0;
}

int main(void)
{
    int rc = test_fs_basic();
    printf("testprocess: %s\n", rc == 0 ? "done" : "fail");
    exit(rc == 0 ? 0 : -1);
}
