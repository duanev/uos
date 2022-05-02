
#include <stdarg.h>
#include "uos.h"

#if MAX_CPUS > 1
static SPMC_GATE_INIT(con_puts, MAX_CPUS);

void
puts(const char * buf)
{
    // make puts re-entrant - con_puts() is not
    u64 token;
    wait_for_token(con_puts, token);

    {
#       ifdef LOG_TSTAMP
        char pbuf[32];       // thread stack
        char * p = pbuf + strlen(etimestr(pbuf));
        *p++ = '[';         // [ appears only for tstamp
#       else
        char pbuf[4];
        char * p = pbuf;
#       endif
        // way useful debug: show which cpu is printing this line
        *p++ = '0' + _cpu_id();
        *p++ = ']';
        *p++ = ' ';
        *p   = '\0';
        con_puts(pbuf);
    }

    con_puts(buf);
    return_token(con_puts, token);
}
#else
void puts(const char * buf) { con_puts(buf); }
#endif

static const char xdigits[16] = { "0123456789abcdef" };

/*
 * _vsprintf() returns a count of the printable characters,
 * but requires buf to be one character larger
 */
static int
_vsprintf(char * buf, const char * fmt, va_list ap)
{
    char nbuf[64];
    char fill = ' ';
    int ljustify = 0;
    int width = 0;
    int mode = 0;           // state: 0 = copying chars, 1 = building format
    int b64 = 0;
    int first;

    char * q = buf;
    for (const char * p = fmt; *p; p++) {
        u64 u;
        char * s;

        if (mode) {
            switch (*p) {
            case 'c':
                u = va_arg(ap, int);
                *q++ = u;
                mode = b64 = width = ljustify = 0;
                fill = ' ';
                break;
            case 'd':
                u = b64 ? va_arg(ap, u64) : va_arg(ap, int);
                s = nbuf;
                do {
                    *s++ = '0' + u % 10;
                    u /= 10;
                } while (u);
                width -= (s - nbuf);
                if (ljustify) {
                    while (s > nbuf)  *q++ = *--s;
                    while (width-- > 0) *q++ = fill;
                } else {
                    while (width-- > 0) *q++ = fill;
                    while (s > nbuf)  *q++ = *--s;
                }
                mode = b64 = width = ljustify = 0;
                fill = ' ';
                break;
            case 'l':
                b64 = 1;
                break;
            case 's':
                s = va_arg(ap, char *);
                if (s == 0) s = "(null)";
                width -= strlen(s);
                if (ljustify) {
                    while (*s)  *q++ = *s++;
                    while (width-- > 0)  *q++ = ' ';
                } else {
                    while (width-- > 0)  *q++ = ' ';
                    while (*s)  *q++ = *s++;
                }
                mode = b64 = width = ljustify = 0;
                fill = ' ';
                break;
            case 'x':
                u = b64 ? va_arg(ap, u64) : va_arg(ap, u32);
                s = nbuf;
                do {
                    *s++ = xdigits[u % 16];
                    u /= 16;
                } while (u);
                width -= (s - nbuf);
                if (ljustify) {
                    while (s > nbuf)  *q++ = *--s;
                    while (width-- > 0) *q++ = fill;
                } else {
                    while (width-- > 0) *q++ = fill;
                    while (s > nbuf)  *q++ = *--s;
                }
                mode = b64 = width = ljustify = 0;
                fill = ' ';
                break;
            case '-':
                ljustify = 1;
                fill = ' ';
                break;
            case '0':
                if (first)
                    fill = '0';
                else
                    width *= 10;
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                width *= 10;
                width += (*p - '0');
                break;
            default:
                *q++ = *p;
                mode = b64 = width = ljustify = 0;
                fill = ' ';
            }
            if (first) first = 0;
        } else {
            if (first) first = 0;
            if (*p == '%') {
                mode = 1;
                first = 1;
            } else {
                *q++ = *p;
            }
        }
    }
    *q = 0;     // don't include the null delimiter in the count ...

    return q - buf;
}

int
sprintf(char * buf, const char * fmt, ...)
{
    int rc;
    va_list ap;
    va_start(ap, fmt);

    rc = _vsprintf(buf, fmt, ap);

    //stkcheck();
    va_end(ap);
    return rc;
}

int
printf(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // for the moment, tls is just a printf buffer
    int rc = _vsprintf(get_tp(), fmt, ap);
    puts(get_tp());

    va_end(ap);
    return rc;
}

static char * Indent = "                                                        ";

int
printi(int depth, const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // for the moment, tls is just a printf buffer
    int rc = _vsprintf(get_tp(), fmt, ap);
    puts(Indent + strlen(Indent) - 1 - depth * 4);
    puts(get_tp());

    va_end(ap);
    return rc;
}


long
strlen(const char * s)
{
    int i = 0;
    while (s[i]) i++;
    return i;
}

char *
strcpy(char * dst, const char * src)
{
    while (*src)
        *dst++ = *src++;
    *dst = 0;
}

int
strcmp(const char * s1, const char * s2)
{
    while (*s1  &&  *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }
    return *s2 - *s1;
}

int
strcmpn(const char * s1, const char * s2, int n)
{
    while (*s1  &&  *s2) {
        if (*s1 != *s2)
            break;
        if (--n < 1)
            return 0;
        s1++;
        s2++;
    }
    return *s2 - *s1;
}

/*
 * memset() and memcpy() are only needed if -mstrict-align is enabled, or
 * if a mem* call is made with a variable length (lengths known at compile
 * time can be unrolled by the compiler); else the __builtin_mem* work.
 * (see global.h)
 */

void *
memset(void * s, int c, unsigned long n)
{
    u8 * d = s;
    while (n-- > 0)
        *d++ = c;
    return s;
}

void *
memcpy(void * dst, void * src, unsigned long n)
{
    u8 * d = dst;
    while (n-- > 0)
        *d++ = *((u8 *)src++);
    return dst;
}

// memclr() currently assumes word aligned start and size
void
memclr(void * vaddr, unsigned long size)
{
    u64 i;
    for (i = 0; i < size; i += sizeof(u64))
        *(u64 *)(vaddr + i) = 0;

    if (i != size) {
        i -= sizeof(u64);
        memset(vaddr + i, 0, size - i);
    }

#if 0
    // I originally wrote the above replacement for memset because it solved
    // unhandled alignment fault (7) that I was seeing with __builtin_memset,
    // but then I found the performance of the above far exceeded memset()!
    memset(vaddr, 0, size);
#endif
}

int
n_bits_set(u64 x)
{
    int n = 0;
    for (int i = 0; i < 64; i++) {
        if (x & 1)
            n++;
        x >>= 1;
    }
    return n;
}

void
hexdump(void * buf, int count, u64 addr)
{
    int     i, column, diff, lastdiff;
    char    hexbuf[49], asciibuf[17], last[17];
    char    *hptr, *aptr;

    hptr = hexbuf;
    aptr = asciibuf;
    i = column = 0;
    lastdiff = 0;
    diff = 1;
    while (i < count) {
        unsigned char b = *((unsigned char *)buf + i);
        if (b != last[column]) diff = 1;
        last[column] = b;
        *hptr++ = xdigits[(b >> 4) & 0xf];
        *hptr++ = xdigits[b & 0xf];
        *hptr++ = column == 7 ? '-' : ' ';
        *aptr++ = (b >= ' ' && b <= '~') ? b : '.';
        if (++column == 16) {
            *hptr = *aptr = 0;
            if (diff == 0) {
                if (lastdiff == 1)
                    printf("(same)\n");
            } else {
                printf("%08lx: %s  %s\n", addr, hexbuf, asciibuf);
            }
            lastdiff = diff;
            diff = 0;
            addr += 16;
            column = 0;
            hptr = hexbuf;
            aptr = asciibuf;
        }
        i++;
    }
    if (column) {
        *hptr = *aptr = 0;
        printf("%08lx: %s  %s\n", addr, hexbuf, asciibuf);
    }
}

