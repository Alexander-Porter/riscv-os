// 基础字符串/内存操作集合，参考 xv6

#include "types.h"

void *memset(void *dst, int c, uint n)
{
  char *cdst = (char *)dst;
  for (uint i = 0; i < n; i++)
    cdst[i] = c;
  return dst;
}

void *memmove(void *dst, const void *src, int n)
{
  const char *s = (const char *)src;
  char *d = (char *)dst;

  if (s > d)
  {
    for (int i = 0; i < n; i++)
      d[i] = s[i];
  }
  else
  {
    for (int i = n - 1; i >= 0; i--)
      d[i] = s[i];
  }
  return dst;
}

char *safestrcpy(char *dst, const char *src, int n)
{
  if (n <= 0)
    return dst;

  int i;
  for (i = 0; i < n - 1 && src[i]; i++)
    dst[i] = src[i];
  dst[i] = '\0';
  return dst;
}

int strncmp(const char *p, const char *q, uint n)
{
  while (n > 0 && *p && *p == *q)
  {
    n--;
    p++;
    q++;
  }

  if (n == 0)
    return 0;
  return *(const unsigned char *)p - *(const unsigned char *)q;
}

int strlen(const char *s)
{
  int n = 0;
  while (s[n])
    n++;
  return n;
}

int strcmp(const char *p, const char *q)
{
  while (*p && *p == *q)
  {
    p++;
    q++;
  }
  return (unsigned char)*p - (unsigned char)*q;
}
