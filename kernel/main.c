#include "global_func.h"
#include "memlayout.h"
#include "types.h"

// 读取time寄存器的内联函数
static inline uint64 r_time()
{
    uint64 x;
    asm volatile("csrr %0, time" : "=r" (x) );
    return x;
}

// 简易断言宏, 条件为假时触发panic
#define assert(x) do { if (!(x)) panic("assertion failed"); } while (0)

// Lab2: printf功能基础测试
void test_printf_basic()
{
    printf("Testing integer: %d\n", 42);
    printf("Testing negative: %d\n", -123);
    printf("Testing zero: %d\n", 0);
    printf("Testing hex: 0x%x\n", 0xABC);
    printf("Testing string: %s\n", "Hello");
    printf("Testing char: %c\n", 'X');
    printf("Testing percent: %%");
}

// Lab2: printf功能边界条件测试
void test_printf_edge_cases()
{
    printf("INT_MAX: %d\n", 2147483647);
    printf("INT_MIN: %d\n", -2147483648);
    printf("NULL string: %s\n", (char *)0);
    printf("Empty string: %s\n", "");
}
int test_max_alloc() {
    int count = 0;
    while (kmalloc(sizeof(char)) != 0)
    {
        /* code */
        count++;
    }
    printf("Max allocable blocks: %d\n", count);
    return 0;
}

void test_many_alloc_free_timings() {
    const int N = 50;
    void* pages[N];
    uint64 start_time, end_time;

    // 分配N页
    start_time = r_time();
    for (int i = 0; i < N; i++) {
        pages[i] = alloc_pages(10);
        if (pages[i] == 0) {
            printf("Allocation failed at %d\n", i);
            break;
        }
    }
    end_time = r_time();
    printf("Allocated %d pages in %d ticks\n", N, end_time - start_time);

    // 释放N页
    start_time = r_time();
    for (int i = 0; i < N; i++) {
        if (pages[i]) {
            free_page(pages[i]);
        }
    }
    end_time = r_time();
    printf("Freed %d pages in %d ticks\n", N, end_time - start_time);
}

// Lab3: 物理内存分配器功能测试
void test_physical_memory() {
printf("\nTesting physical memory allocator...\n");

// 测试基本分配和释放
void *page1 = alloc_page();
void *page2 = alloc_page();
printf("alloc page1=%p page2=%p\n", page1, page2);
assert(page1 != page2);
assert(((uint64)page1 & 0xFFF) == 0);  // 页对齐检查
assert(((uint64)page2 & 0xFFF) == 0);  // 页对齐检查
printf("  - allocation and alignment test passed\n");

// 测试数据写入
*(int*)page1 = 0x12345678;
assert(*(int*)page1 == 0x12345678);
printf("  - data write test passed\n");

// 测试释放和重新分配
free_page(page1);
printf("freed page1=%p\n", page1);
void *page3 = alloc_page();
printf("realloc page3=%p\n", page3);
// page3可能等于page1（取决于分配策略）

free_page(page2);
free_page(page3);
printf("Physical memory allocator tests passed!\n");
}

// 内核主函数
void main()
{
    clear_screen();
    
    printf("--- Running Lab 2 tests ---\n");
    test_printf_basic();
    test_printf_edge_cases();
    printf("\n--- Lab 2 tests passed ---\n");

    printf("\n--- Running Lab 3 setup ---\n");
    pmm_init();         // 初始化物理内存管理器
    test_physical_memory(); // 测试物理内存分配
    kvm_init();         // 创建内核页表
    kvm_init_hart();    // 启用分页
    printf("--- Lab 3 setup finished, paging enabled ---\n");

    printf("\n--- Running Lab 3 tests (post-paging) ---\n");
    test_physical_memory(); // 在分页启用后再次测试物理内存分配
    printf("--- Lab 3 tests passed ---\n");

    printf("\nAll tests passed. Halting.\n");
    test_many_alloc_free_timings();
    test_max_alloc();
    
    while(1);
}
