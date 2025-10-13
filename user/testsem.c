#include "user.h"

#define ITER 5

int main(void)
{
    printf("testsem: start\n");

    int sem_ping = sem_create(1);
    int sem_pong = sem_create(0);
    if (sem_ping < 0 || sem_pong < 0)
    {
        printf("testsem: sem_create failed\n");
        exit(-1);
    }

    int pid = fork();
    if (pid < 0)
    {
        printf("testsem: fork failed\n");
        exit(-1);
    }

    if (pid == 0)
    {
        // 子进程等待父进程放行，再释放父进程的信号量形成乒乓
        for (int i = 0; i < ITER; i++)
        {
            if (sem_wait(sem_pong) < 0)
            {
                printf("testsem: child sem_wait failed\n");
                exit(-1);
            }
            printf("testsem: child step %d\n", i);
            if (sem_post(sem_ping) < 0)
            {
                printf("testsem: child sem_post failed\n");
                exit(-1);
            }
        }
        printf("testsem: child done\n");
        exit(0);
    }

    for (int i = 0; i < ITER; i++)
    {
        if (sem_wait(sem_ping) < 0)
        {
            printf("testsem: parent sem_wait failed\n");
            exit(-1);
        }
        printf("testsem: parent step %d\n", i);
        if (sem_post(sem_pong) < 0)
        {
            printf("testsem: parent sem_post failed\n");
            exit(-1);
        }
    }

    if (wait(0) < 0)
    {
        printf("testsem: wait failed\n");
        exit(-1);
    }

    printf("testsem: success\n");
    exit(0);
}
