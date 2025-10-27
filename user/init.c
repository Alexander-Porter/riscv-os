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
    printf("init: starting tests\n");
    // 依次运行的用户程序列表
    static const char *const tests[] = {
        "testprocess",    // 基础：创建/写入/读取/stat/目录
        "testsyscall2",   // 链接/解除链接与内容验证
        "testcow",        // 目录与相对路径
        "testsbrkbench",  // 并发小文件创建/写入
        "testfsperf"      // 性能测试（小文件+大文件）
    };


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
