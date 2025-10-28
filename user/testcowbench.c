// COW Fork 性能基准测试
#include "user.h"

#define TEST_PAGES 100  // 测试页面数
#define PAGE_SIZE 4096

// 测试1：fork时间对比
static void test_fork_time(void) {
    printf("=== Test 1: Fork Time ===\n");
    
    // 分配内存
    char *mem = sbrk(TEST_PAGES * PAGE_SIZE);
    if ((uint64)mem == 0xffffffffffffffff) {
        printf("sbrk failed\n");
        exit(-1);
    }
    
    // 写入数据
    for (int i = 0; i < TEST_PAGES * PAGE_SIZE; i += PAGE_SIZE) {
        mem[i] = 'A';
    }
    
    uint64 t0 = rdtime();
    int pid = fork();
    uint64 t1 = rdtime();
    
    if (pid < 0) {
        printf("fork failed\n");
        exit(-1);
    }
    
    if (pid == 0) {
        // 子进程：不修改内存，直接退出
        exit(0);
    } else {
        // 父进程：等待子进程
        wait(0);
        printf("Fork time (COW): %lu ticks\n", t1 - t0);
        printf("Memory size: %d KB\n", TEST_PAGES * 4);
    }
}

// 测试2：COW触发时的页错误开销
static void test_cow_pagefault(void) {
    printf("\n=== Test 2: COW Page Fault Overhead ===\n");
    
    char *mem = sbrk(TEST_PAGES * PAGE_SIZE);
    if ((uint64)mem == 0xffffffffffffffff) {
        printf("sbrk failed\n");
        exit(-1);
    }
    
    // 初始化内存
    for (int i = 0; i < TEST_PAGES * PAGE_SIZE; i += PAGE_SIZE) {
        mem[i] = 'B';
    }
    
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(-1);
    }
    
    if (pid == 0) {
        // 子进程：触发COW写入
        uint64 t0 = rdtime();
        for (int i = 0; i < TEST_PAGES * PAGE_SIZE; i += PAGE_SIZE) {
            mem[i] = 'C';  // 触发COW
        }
        uint64 t1 = rdtime();
        printf("Child COW write time: %lu ticks for %d pages\n", 
               t1 - t0, TEST_PAGES);
        exit(0);
    } else {
        wait(0);
    }
}

// 测试3：内存共享效率
static void test_memory_sharing(void) {
    printf("\n=== Test 3: Memory Sharing Efficiency ===\n");
    
    char *mem = sbrk(TEST_PAGES * PAGE_SIZE);
    if ((uint64)mem == 0xffffffffffffffff) {
        printf("sbrk failed\n");
        exit(-1);
    }
    
    // 初始化
    for (int i = 0; i < TEST_PAGES * PAGE_SIZE; i += PAGE_SIZE) {
        mem[i] = 'D';
    }
    
    printf("Parent allocated: %d KB\n", TEST_PAGES * 4);
    
    int pid = fork();
    if (pid == 0) {
        // 子进程：只读取，不写入（完全共享）
        char sum = 0;
        for (int i = 0; i < TEST_PAGES * PAGE_SIZE; i += PAGE_SIZE) {
            sum += mem[i];
        }
        printf("Child read-only access, sum=%d (memory shared)\n", (int)sum);
        exit(0);
    } else {
        wait(0);
        printf("COW allows child to share parent's memory without copying\n");
    }
}

// 测试4：多进程场景
static void test_multi_process(void) {
    printf("\n=== Test 4: Multi-Process Scenario ===\n");
    
    #define NUM_CHILDREN 4
    
    char *mem = sbrk(50 * PAGE_SIZE);
    if ((uint64)mem == 0xffffffffffffffff) {
        printf("sbrk failed\n");
        exit(-1);
    }
    
    for (int i = 0; i < 50 * PAGE_SIZE; i += PAGE_SIZE) {
        mem[i] = 'E';
    }
    
    uint64 t0 = rdtime();
    
    for (int i = 0; i < NUM_CHILDREN; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("fork %d failed\n", i);
            exit(-1);
        }
        if (pid == 0) {
            // 子进程：修改一小部分内存
            for (int j = 0; j < 10 * PAGE_SIZE; j += PAGE_SIZE) {
                mem[j] = 'F';
            }
            exit(0);
        }
    }
    
    // 等待所有子进程
    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(0);
    }
    
    uint64 t1 = rdtime();
    printf("Created %d children with 200KB memory in %lu ticks\n", 
           NUM_CHILDREN, t1 - t0);
    printf("Each child modified only 40KB (COW optimization)\n");
}

int main(void) {
    printf("=== COW Fork Performance Benchmark ===\n\n");
    
    test_fork_time();
    test_cow_pagefault();
    test_memory_sharing();
    test_multi_process();
    
    printf("\n=== Benchmark Complete ===\n");
    printf("COW优化总结：\n");
    printf("1. Fork时间短（只复制页表，不复制内存）\n");
    printf("2. 内存共享效率高（只在写入时复制）\n");
    printf("3. 多进程场景下内存使用更少\n");
    printf("4. 页错误处理有少量开销，但总体收益显著\n");
    
    if (getpid() == 1) {
        for (;;) sleep(1000);
    }
    exit(0);
}
