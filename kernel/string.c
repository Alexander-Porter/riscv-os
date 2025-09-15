// 基础字符串操作 (string.c)

#include "types.h"

// 将dst指向的内存区域的前n个字节设置为值c
void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  for(uint i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

// 安全地拷贝n个字节, 能正确处理内存重叠的情况。
//
// 为什么需要memmove?
// memcpy的行为在源和目标区域重叠时是未定义的, 这可能导致数据损坏。
// memmove通过检查内存区域是否重叠来保证拷贝的正确性。
//
// 实现逻辑:
// 1. 检测重叠: `if(s < d && s + n > d)`
//    这个条件判断目标地址`d`是否在源地址`s`之后, 并且源和目标区域有重叠。
//    - `s < d`: 目标在源之后。
//    - `s + n > d`: 源的末尾超过了目标的开头, 意味着有重叠。
// 2. 如果重叠, 从后向前拷贝:
//    为了避免覆盖尚未拷贝的源数据, 我们从两个区域的末尾开始, 向前逐字节拷贝。
// 3. 如果不重叠, 从前向后拷贝:
//    这是最高效的拷贝方式。
void*
memmove(void *dst, const void *src, uint n)
{
  const char *s = src;
  char *d = dst;
  if(s < d && s + n > d){
    s += n;
    d += n;
    while(n-- > 0)
      *--d = *--s;
  } else {
    while(n-- > 0)
      *d++ = *s++;
  }
  return dst;
}
