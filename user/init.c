#include "user.h"

// 启动子进程并切换到指定的用户程序
static void run_child(const char *prog)
{
    char *argv[2];
    argv[0] = (char *)prog;
    argv[1] = 0;

    exec(prog, argv);
    printf("init: exec %s failed\n", prog);
    exit(-1);
}

int main(void)
{
    // 缺省：Lab7 文件系统统一测试；若指定 TESTALL_INIT，则改为附加功能综合测试
#ifdef TESTALL_INIT
    printf("init: start testall (附加功能)\n");
    static const char *const tests[] = { "testall" };
#else
    printf("init: start testfsall\n");
    // 本实验按指南要求：在 init 内部直接运行统一的文件系统测试 testfsall
    // 这样可以在一次引导中完成基本/并发/性能三类用例，输出更集中
    static const char *const tests[] = { "testfsall" };
#endif


    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf("init: fork for %s failed\n", tests[i]);
            continue;
        }

        if (pid == 0)
        {
            run_child(tests[i]);
        }

        int status = 0;
        if (wait(&status) < 0)
        {
            printf("init: wait for %s failed\n", tests[i]);
        }
        else
        {
            printf("init: %s exited with %d\n", tests[i], status);
        }
    }

    printf("init: all tests finished\n");
    for (;;)
    {
        sleep(1000);
    }
}
