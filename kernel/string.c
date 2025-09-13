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
