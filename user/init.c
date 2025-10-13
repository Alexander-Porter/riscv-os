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
    // 依次运行的用户程序列表
    static const char *const tests[] = {
        "testsyscall",
        "testfork",
        "testsched",
        "testsem",
        "testcow",
    };

    printf("init: entering test harness\n");

    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        int pid = fork();
        printf("init: after fork pid=%d (self=%d) for %s\n", pid, getpid(), tests[i]);
        if (pid < 0)
        {
            printf("init: fork for %s failed\n", tests[i]);
            continue;
        }

        if (pid == 0)
        {
            printf("init: launching %p (%s)\n", tests[i], tests[i]);
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
