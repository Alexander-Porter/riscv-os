#include "user.h"

static inline uint64 read_time(void)
{
    return rdtime();
}

// 填充缓冲区，帮助验证 sbrk 返回的地址可读写
static void fill_buffer(char *buf, int len)
{
    for (int i = 0; i < len; i++)
        buf[i] = 'A' + (i % 26);
}

// 覆盖 fork/wait/sbrk 等基础系统调用
int test_basic_syscalls(void)
{
    printf("test_basic_syscalls: pid=%d\n", getpid());

    int child = fork();
    if (child < 0)
    {
        printf("test_basic_syscalls: fork failed\n");
        return -1;
    }
    if (child == 0)
    {
        printf("test_basic_syscalls: child running pid=%d\n", getpid());
        sleep(20);
        exit(123);
    }
    int status = 0;
    wait(&status);
    printf("test_basic_syscalls: wait status=%d\n", status);

    char *mem = sbrk(128);
    if (mem == SBRK_ERROR)
    {
        printf("test_basic_syscalls: sbrk failed\n");
        return -1;
    }
    fill_buffer(mem, 128);
    write(1, "test_basic_syscalls: sbrk ok\n", 30);
    return 0;
}

// 检查参数边界与错误场景
int test_parameter_passing(void)
{
    printf("test_parameter_passing: begin\n");
    const char *msg = "parameter passing works\n";
    int ret = write(1, msg, (int)strlen(msg));
    printf("test_parameter_passing: write ret=%d\n", ret);

    ret = write(1, msg, -1);
    printf("test_parameter_passing: write negative len ret=%d\n", ret);

    ret = write(1, (void *)0, 4);
    printf("test_parameter_passing: write null ptr ret=%d\n", ret);
    return 0;
}

// 尝试非法指针与无效文件描述符
int test_security(void)
{
    printf("test_security: begin\n");
    char *invalid = (char *)0xffff0000UL;
    int ret = write(1, invalid, 16);
    printf("test_security: invalid pointer ret=%d\n", ret);

    ret = write(99, "bad fd\n", 7);
    printf("test_security: bad fd ret=%d\n", ret);
    return 0;
}

// 粗略统计系统调用开销
int test_syscall_performance(void)
{
    printf("test_syscall_performance: begin\n");
    uint64 before = read_time();
    for (int i = 0; i < 10000; i++)
        getpid();
    uint64 after = read_time();
    uint64 delta = after - before;
    printf("test_syscall_performance: 10000 getpid cost %x cycles\n", (unsigned int)delta);
    return 0;
}

int main(void)
{
    test_basic_syscalls();
    sleep(250);
    test_parameter_passing();
    sleep(250);
    test_security();
    sleep(250);
    test_syscall_performance();
    sleep(250);
    printf("testsyscall2: done\n");
    exit(0);
}
