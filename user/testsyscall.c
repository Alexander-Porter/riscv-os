#include "user.h"

int main(void)
{
    printf("testsyscall: pid=%d\n", getpid());

    printf("testsyscall: sleeping\n");
    sleep(50);
    printf("testsyscall: woke up\n");

    if (yield() < 0)
        printf("testsyscall: yield failed\n");

    printf("testsyscall: done\n");
    exit(0);
}
