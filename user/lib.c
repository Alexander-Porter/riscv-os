#include "user.h"

void *memset(void *dst, int c, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n-- > 0)
        *p++ = (unsigned char)c;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (s >= d)
    {
        while (n-- > 0)
            *d++ = *s++;
    }
    else
    {
        d += n;
        s += n;
        while (n-- > 0)
            *--d = *--s;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

int strcmp(const char *lhs, const char *rhs)
{
    while (*lhs && (*lhs == *rhs))
    {
        lhs++;
        rhs++;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++) != '\0')
        ;
    return ret;
}

int strncmp(const char *lhs, const char *rhs, size_t n)
{
    while (n-- > 0)
    {
        if (*lhs != *rhs || *lhs == '\0')
            return (unsigned char)*lhs - (unsigned char)*rhs;
        lhs++;
        rhs++;
    }
    return 0;
}

int atoi(const char *s)
{
    int sign = 1;
    int value = 0;

    if (*s == '-')
    {
        sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9')
    {
        value = value * 10 + (*s - '0');
        s++;
    }
    return sign * value;
}
