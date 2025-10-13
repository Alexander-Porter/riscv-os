#include "user.h"

int main(void)
{
    printf("testfork: parent start\n");

    int pid = fork();
    if (pid < 0)
    {
        printf("testfork: fork failed\n");
        exit(-1);
    }

    if (pid == 0)
    {
        printf("Child: after fork\n");
        exit(0);
    }

    printf("Parent: forked child %d\n", pid);

    int status = 0;
    if (wait(&status) < 0)
    {
        printf("testfork: wait failed\n");
        exit(-1);
    }

    printf("testfork: child exited with %d\n", status);
    printf("testfork: done\n");
    exit(0);
}
