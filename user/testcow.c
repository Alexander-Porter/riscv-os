#include "user.h"

#define PAGE (4096)

static void fail(const char *msg)
{
    printf("testcow: %s\n", msg);
    exit(-1);
}

int main(void)
{
    printf("testcow: start\n");

    char *mem = sbrk(PAGE);
    if (mem == (char *)-1)
        fail("sbrk failed");

    mem[0] = 0x11;           // 先写入父进程可见的数据
    mem[PAGE / 2] = 0x22;    // 触发页分配

    int pid = fork();
    if (pid < 0)
        fail("fork failed");

    if (pid == 0)
    {
        if (mem[0] != 0x11 || mem[PAGE / 2] != 0x22)
            fail("child sees wrong initial values");

        mem[0] = 0x33;       // 子进程写入，期待触发写时复制
        mem[PAGE / 2] = 0x44;

        if (mem[0] != 0x33 || mem[PAGE / 2] != 0x44)
            fail("child write failed");

        printf("testcow: child ok\n");
        exit(0);
    }

    if (wait(0) < 0)
        fail("wait failed");

    if (mem[0] != 0x11 || mem[PAGE / 2] != 0x22)
        fail("parent page modified after child");

    mem[PAGE - 1] = 0x55;    // 再写一次，确保写回正常

    int pid2 = fork();
    if (pid2 < 0)
        fail("second fork failed");

    if (pid2 == 0)
    {
        if (mem[PAGE - 1] != 0x55)
            fail("second child inherited wrong data");
        mem[PAGE - 1] = 0x77;
        if (mem[PAGE - 1] != 0x77)
            fail("second child write failed");
        exit(0);
    }

    if (wait(0) < 0)
        fail("wait second child failed");

    if (mem[PAGE - 1] != 0x55)
        fail("parent data overwritten by second child");

    printf("testcow: success\n");
    exit(0);
}
