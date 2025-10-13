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

void vprintf(const char *fmt, va_list ap)
{
    const char *p;
    for (p = fmt; *p; p++)
    {
        if (*p != '%')
        {
            putc(1, *p);
            continue;
        }
        p++;
        if (!*p)
            break;
        switch (*p)
        {
        case 'd':
        case 'i':
            printint(1, va_arg(ap, int), 10, 1);
            break;
        case 'u':
            printint(1, va_arg(ap, unsigned int), 10, 0);
            break;
        case 'x':
        case 'X':
            printint(1, va_arg(ap, unsigned int), 16, 0);
            break;
        case 'p':
            printptr(1, va_arg(ap, uint64));
            break;
        case 's':
        {
            const char *s = va_arg(ap, const char *);
            if (s == 0)
                s = "(null)";
            while (*s)
                putc(1, *s++);
            break;
        }
        case 'c':
            putc(1, (char)va_arg(ap, int));
            break;
        case '%':
            putc(1, '%');
            break;
        default:
            putc(1, '%');
            putc(1, *p);
            break;
        }
    }
}

void printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void fprintf(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const char *p;
    for (p = fmt; *p; p++)
    {
        if (*p != '%')
        {
            putc(fd, *p);
            continue;
        }
        p++;
        if (!*p)
            break;
        switch (*p)
        {
        case 'd':
        case 'i':
            printint(fd, va_arg(ap, int), 10, 1);
            break;
        case 'u':
            printint(fd, va_arg(ap, unsigned int), 10, 0);
            break;
        case 'x':
        case 'X':
            printint(fd, va_arg(ap, unsigned int), 16, 0);
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
            putc(fd, '%');
            putc(fd, *p);
            break;
        }
    }
    va_end(ap);
}
