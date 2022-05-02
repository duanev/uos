
void con_puts(const char *);
int  con_peek(void);
char con_getc(void);

// elapsed usecs since boot
static inline u64
etime(void)
{
    u64 cntpct;
    asm volatile("mrs %0, cntpct_el0" : "=r" (cntpct));
    cntpct /= 64;
    return cntpct;
}

