#include "user.h"

static void busy_loop(int loops)
{
    volatile int sink = 0;
    for (int i = 0; i < loops; i++)
    {
        sink += i;
        if ((i & 0x3fff) == 0)
            asm volatile("");
    }
    if (sink == -1)
        fprintf(2, "sink hit\n");
}

static void run_worker(const char *label, int priority, int loops)
{
    if (setpriority(priority) < 0)
    {
        fprintf(2, "%s: setpriority failed\n", label);
        exit(-1);
    }

    int actual = getpriority();
    printf("%s priority set to %d\n", label, actual);

    busy_loop(loops);

    uint64 ticks = getrunticks();
    printf("%s ticks=%d\n", label, (int)ticks);
    exit(0);
}

int main(void)
{
    printf("testsched: parent start\n");

    int pid_high = fork();
    if (pid_high < 0)
    {
        printf("testsched: fork high failed\n");
        exit(-1);
    }
    if (pid_high == 0)
        run_worker("high priority", 0, 20000000);

    int pid_low = fork();
    if (pid_low < 0)
    {
        printf("testsched: fork low failed\n");
        exit(-1);
    }
    if (pid_low == 0)
        run_worker("low priority", 2, 40000000);

    for (int i = 0; i < 2; i++)
    {
        int status = 0;
        int pid = wait(&status);
        if (pid < 0)
        {
            printf("testsched: wait failed\n");
            exit(-1);
        }
        printf("testsched: child %d exit status %d\n", pid, status);
    }

    printf("testsched: done\n");
    exit(0);
}
