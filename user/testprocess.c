#include "user.h"

#define SBRK_PAGES 1000
#define PAGE_SIZE 4096

#define BUFFER_CAPACITY 4

static int buffer[BUFFER_CAPACITY];
static int write_index = 0;
static int read_index = 0;
static int sem_empty = -1;
static int sem_full = -1;
static int sem_mutex = -1;

static void shared_buffer_init(void)
{
    sem_empty = sem_create(BUFFER_CAPACITY);
    sem_full = sem_create(0);
    sem_mutex = sem_create(1);
    write_index = 0;
    read_index = 0;
}

static void shared_buffer_put(int value)
{
    sem_wait(sem_empty);
    sem_wait(sem_mutex);
    buffer[write_index % BUFFER_CAPACITY] = value;
    write_index++;
    sem_post(sem_mutex);
    sem_post(sem_full);
}

static int shared_buffer_get(void)
{
    sem_wait(sem_full);
    sem_wait(sem_mutex);
    int value = buffer[read_index % BUFFER_CAPACITY];
    read_index++;
    sem_post(sem_mutex);
    sem_post(sem_empty);
    return value;
}

static int sbrk_workload(const char *tag)
{
    for (int i = 0; i < SBRK_PAGES; i++)
    {
        char *mem = sbrk(PAGE_SIZE);
        if (mem == SBRK_ERROR)
        {
            printf("%s: sbrk failed at page %d\n", tag, i);
            return -1;
        }

        memset(mem, 0x5a, PAGE_SIZE); // 触发实际分配
    }
    return 0;
}

static void child_worker(const char *tag, int priority)
{
    setpriority(priority);
    printf("%s: pid=%d priority=%d\n", tag, getpid(), getpriority());

    uint64 wall_start = uptime();
    if (sbrk_workload(tag) < 0)
        exit(-1);
    uint64 wall_end = uptime();

    uint64 wall_delta = wall_end - wall_start;
    printf("%s: wall_time=%u ms\n", tag, (unsigned int)wall_delta);
    exit(0);
}

static int spawn_child(const char *tag, int priority)
{
    int pid = fork();
    if (pid < 0)
        return pid;
    if (pid == 0)
        child_worker(tag, priority);
    return pid;
}

int test_process_creation(void)
{
    printf("test_process_creation: begin\n");
    int created = 0;
    for (int i = 0; i < 4; i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf("test_process_creation: fork failed at %d\n", i);
            break;
        }
        if (pid == 0)
        {
            printf("test_process_creation: child %d pid=%d\n", i, getpid());
            sleep(10);
            exit(10 + i);
        }
        created++;
    }

    for (int i = 0; i < created; i++)
    {
        int status = 0;
        int pid = wait(&status);
        printf("test_process_creation: child %d exited status=%d\n", pid, status);
    }

    printf("test_process_creation: end created=%d\n", created);
    return 0;
}

int test_scheduler(void)
{
    printf("test_scheduler: begin\n");
    int pid_high = spawn_child("sched-high", 0);
    int pid_mid = spawn_child("sched-mid", 1);
    int pid_low = spawn_child("sched-low", 2);

    int pids[3] = {pid_high, pid_mid, pid_low};
    for (int i = 0; i < 3; i++)
    {
        if (pids[i] < 0)
            printf("test_scheduler: spawn child %d failed\n", i);
    }

    for (int i = 0; i < 3; i++)
    {
        int status = 0;
        int pid = wait(&status);
        if (pid >= 0)
            printf("test_scheduler: wait pid=%d status=%d\n", pid, status);
    }
    printf("test_scheduler: end\n");
    return 0;
}

int test_synchronization(void)
{
    printf("test_synchronization: begin\n");
    shared_buffer_init();
    if (sem_empty < 0 || sem_full < 0 || sem_mutex < 0)
    {
        printf("test_synchronization: semaphore create failed\n");
        return -1;
    }

    int pid = fork();
    if (pid < 0)
    {
        printf("test_synchronization: fork failed\n");
        return -1;
    }

    if (pid == 0)
    {
        for (int i = 0; i < 5; i++)
        {
            int value = shared_buffer_get();
            printf("consumer: step=%d pid=%d value=%d\n", i, getpid(), value);
            sleep(100);
        }
        exit(0);
    }

    for (int i = 0; i < 5; i++)
    {
        printf("producer: step=%d\n", i);
        shared_buffer_put(i);
        sleep(50);
    }

    int status = 0;
    wait(&status);
    printf("test_synchronization: child status=%d\n", status);
    return 0;
}

int main(void)
{
    test_process_creation();
    test_scheduler();
    test_synchronization();
    printf("testprocess: done\n");
    exit(0);
}
