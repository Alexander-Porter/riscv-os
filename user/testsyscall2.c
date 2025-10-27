// 替换为硬链接/解除链接用例：link/unlink/read 验证
#include "user.h"

int main(void)
{
    const char *a = "a";
    const char *b = "b";
    const char *payload = "LINKDATA";

    int fd = open(a, O_CREATE | O_RDWR);
    if (fd < 0) { printf("linktest: create a failed\n"); exit(-1); }
    if (write(fd, payload, (int)strlen(payload)) != (int)strlen(payload)) { printf("linktest: write a failed\n"); exit(-1); }
    close(fd);

    if (link(a, b) < 0) { printf("linktest: link a->b failed\n"); exit(-1); }
    if (unlink(a) < 0) { printf("linktest: unlink a failed\n"); exit(-1); }

    fd = open(b, O_RDONLY);
    if (fd < 0) { printf("linktest: open b failed\n"); exit(-1); }
    char buf[32] = {0};
    int r = read(fd, buf, sizeof(buf));
    if (r != (int)strlen(payload) || strncmp(buf, payload, r) != 0) { printf("linktest: verify b failed r=%d\n", r); exit(-1); }
    close(fd);

    if (unlink(b) < 0) { printf("linktest: unlink b failed\n"); exit(-1); }
    printf("testfs_link: PASS\n");
    exit(0);
}
