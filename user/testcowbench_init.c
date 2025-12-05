// 用于运行testcowbench基准测试的初始化程序
#include "user.h"

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
    printf("=== Starting COW Fork-Exec Benchmark ===\n");
    
    int pid = fork();
    if (pid < 0)
    {
        printf("init: fork failed\n");
        exit(-1);
    }

    if (pid == 0)
    {
        // 子进程运行基准测试
        run_child("testcowbench");
    }

    // 父进程等待子进程完成
    int status = 0;
    if (wait(&status) < 0)
    {
        printf("init: wait failed\n");
    }
    else
    {
        printf("init: testcowbench exited with status %d\n", status);
    }
    
    // 保持系统运行
    for (;;) {
        sleep(1000);
    }
    
    exit(0);
}
