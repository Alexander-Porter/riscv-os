// 替换为目录与相对路径用例：mkdir/chdir/相对路径创建/读取/清理
#include "user.h"

int main(void)
{
    if (mkdir("x") < 0) { printf("dirt: mkdir x failed\n"); exit(-1); }
    if (chdir("x") < 0) { printf("dirt: chdir x failed\n"); exit(-1); }

    int fd = open("y", O_CREATE | O_RDWR);
    if (fd < 0) { printf("dirt: create y failed\n"); exit(-1); }
    if (write(fd, "Q", 1) != 1) { printf("dirt: write y failed\n"); exit(-1); }
    close(fd);

    if (chdir("..") < 0) { printf("dirt: chdir .. failed\n"); exit(-1); }

    fd = open("x/y", O_RDONLY);
    if (fd < 0) { printf("dirt: open x/y failed\n"); exit(-1); }
    char c;
    if (read(fd, &c, 1) != 1 || c != 'Q') { printf("dirt: read x/y failed\n"); exit(-1); }
    close(fd);

    if (unlink("x/y") < 0) { printf("dirt: unlink x/y failed\n"); exit(-1); }
    if (unlink("x") < 0) { printf("dirt: rmdir x failed\n"); exit(-1); }

    printf("testfs_dir: PASS\n");
    exit(0);
}
