#include "global_func.h"
#include "memlayout.h"
#include "types.h"

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

// Lab3: 物理内存分配器功能测试
void test_physical_memory() {
  printf("\nTesting physical memory allocator...\n");
  void *p1 = alloc_page();
  void *p2 = alloc_page();
  assert(p1 != 0); // 测试分配是否成功
  assert(p2 != 0);
  assert(p1 != p2); // 测试两次分配的地址是否不同

  printf("  - allocation test passed\n");

  free_page(p1);
  free_page(p2);

  void *p3 = alloc_page();
  void *p4 = alloc_page();
  assert(p3 != 0);
  assert(p4 != 0);
  // 由于空闲链表是后进先出(LIFO), 后释放的p1应该先被分配出来
  assert(p4 == p1);
  assert(p3 == p2);

  printf("  - free and re-allocation test passed\n");

  // 测试页对齐
  assert(((uint64)p3 & (PGSIZE - 1)) == 0);
  assert(((uint64)p4 & (PGSIZE - 1)) == 0);
  printf("  - alignment test passed\n");

  printf("Physical memory allocator tests passed!\n");
  free_page(p3);
  free_page(p4);
}

// 内核主函数
void main()
{
    clear_screen();
    
    printf("--- Running Lab 2 tests ---\n");
    test_printf_basic();
    test_printf_edge_cases();
    printf("\n--- Lab 2 tests passed ---\n");

    printf("\n--- Running Lab 3 tests ---\n");
    pmm_init(); // 初始化物理内存管理器
    test_physical_memory(); // 测试物理内存管理器
    printf("--- Lab 3 tests passed ---\n");

    printf("\nAll tests passed. Halting.\n");
    while(1);
}
