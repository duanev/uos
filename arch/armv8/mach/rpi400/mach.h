
void con_puts(const char *);

// elapsed usecs since boot
static inline u64
etime(void)
{
    u64 cntpct;
    asm volatile("mrs %0, cntpct_el0" : "=r" (cntpct));
    cntpct /= 64;
    return cntpct;
}

