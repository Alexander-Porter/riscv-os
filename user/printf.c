#include "user.h"
#include <stdarg.h>

static void putc(int fd, char c)
{
    write(fd, &c, 1);
}

static void printint(int fd, long x, int base, int sign)
{
    static const char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;

    unsigned long ux;
    if (sign && x < 0)
        ux = (unsigned long)(-x);
    else
        ux = (unsigned long)x;

    do
    {
        buf[i++] = digits[ux % base];
        ux /= base;
    } while (ux != 0);

    if (sign && x < 0)
        buf[i++] = '-';

    while (--i >= 0)
        putc(fd, buf[i]);
}

static void printptr(int fd, uint64 x)
{
    putc(fd, '0');
    putc(fd, 'x');
    for (int i = (sizeof(uint64) * 2) - 1; i >= 0; i--)
    {
        int nibble = (x >> (i * 4)) & 0xF;
        putc(fd, "0123456789abcdef"[nibble]);
    }
}

static void vformat(int fd, const char *fmt, va_list ap)
{
    for (const char *p = fmt; *p; p++)
    {
        if (*p != '%')
        {
            putc(fd, *p);
            continue;
        }
        p++;
        if (!*p)
            break;

        int is_long = 0;
        if (*p == 'l')
        {
            is_long = 1;
            p++;
            if (!*p) break;
        }

        switch (*p)
        {
        case 'd':
        case 'i':
            if (is_long)
                printint(fd, va_arg(ap, long), 10, 1);
            else
                printint(fd, va_arg(ap, int), 10, 1);
            break;
        case 'u':
            if (is_long)
                printint(fd, (long)va_arg(ap, unsigned long), 10, 0);
            else
                printint(fd, (long)va_arg(ap, unsigned int), 10, 0);
            break;
        case 'x':
        case 'X':
            if (is_long)
                printint(fd, (long)va_arg(ap, unsigned long), 16, 0);
            else
                printint(fd, (long)va_arg(ap, unsigned int), 16, 0);
            break;
        case 'p':
            printptr(fd, va_arg(ap, uint64));
            break;
        case 's':
        {
            const char *s = va_arg(ap, const char *);
            if (s == 0)
                s = "(null)";
            while (*s)
                putc(fd, *s++);
            break;
        }
        case 'c':
            putc(fd, (char)va_arg(ap, int));
            break;
        case '%':
            putc(fd, '%');
            break;
        default:
            // 未支持的格式，按原样输出
            putc(fd, '%');
            if (is_long) putc(fd, 'l');
            putc(fd, *p);
            break;
        }
    }
}

void vprintf(const char *fmt, va_list ap)
{
    vformat(1, fmt, ap);
}

void printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vformat(1, fmt, ap);
    va_end(ap);
}

void fprintf(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vformat(fd, fmt, ap);
    va_end(ap);
}
