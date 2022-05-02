
void    puts(const char * s);
long    strlen(const char * s);
int     strcmp(const char * s1, const char * s2);
char *  strcpy(char * dst, const char * src);
int     sprintf(char * buf, const char * fmt, ...);
int     printf(const char * fmt, ...);
void *  memset(void * s, int c, unsigned long n);
void *  memcpy(void * dst, void * src, unsigned long n);
void    memclr(void * vaddr, unsigned long size);
int     n_bits_set(u64 x);


/* -------- atomic functions -------- */

#define sync_fetch_inc(x) __sync_fetch_and_add(&(x),  1)
#define sync_fetch_dec(x) __sync_fetch_and_add(&(x), -1)

// https://github.com/ARMmbed/core-util/issues/76
// http://zhiyisun.github.io/2016/07/06/Builtin-Atomic-Operation-in-GCC-on-Different-ARM-Arch-Platform.html
// https://stackoverflow.com/questions/21535058/arm64-ldxr-stxr-vs-ldaxr-stlxr

#define atomic_fetch_inc(x) __atomic_fetch_add(&(x), (unsigned int)1, __ATOMIC_ACQ_REL)
#define atomic_dec_fetch(x) __atomic_add_fetch(&(x), (unsigned int)-1, __ATOMIC_ACQ_REL)
#define atomic_fetch(x)     __atomic_load_n(&(x), __ATOMIC_SEQ_CST)
#define atomic_store(x, v)  __atomic_store_n(&(x), (v), __ATOMIC_SEQ_CST)
#define atomic_clear(x)     __atomic_clear(&(x), __ATOMIC_SEQ_CST)
#define atomic_inc(x)       atomic_fetch_inc(x)
#define atomic_dec(x)       atomic_dec_fetch(x)

