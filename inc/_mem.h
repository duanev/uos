
extern int _free_mem;           // all machines using libmem should define the beginning
                                // of free ram in each *.ld

void    mem_init(void);         // not in libmem.c -- look in the machine specific mem_init.c

struct mem_pool {
    u64           base;
    u64           size;
    u64           usize;
    u16           cstk;
    u16           cache[11];    // 11 makes struct size 64
    char *        name;
    struct bitmap map;          // must be last - bitmap blks array at end ...
};

extern struct mem_pool * pool32m;
extern struct mem_pool * pool128k;
extern struct mem_pool * pool4k;

void   mem_init(void);
u64    mem_alloc(struct mem_pool * pool, int n);
u64    mem_zalloc(struct mem_pool * pool, int n);
int    mem_is_in(struct mem_pool * pool, u64 addr);
int    mem_free(struct mem_pool * pool, u64 addr, int n);
int    mem_zfree(struct mem_pool * pool, u64 addr, int n);
void   mem_pool_debug(struct mem_pool * pool);

struct mem_pool * mem_pool_create(char * name, u64 base, u64 size, u64 usize, int zero);


/*
 * since most bits of a slab aligned memory address will be zeros,
 * create an unique "index" based on the address for storage in a
 * much smaller element.  for example, if there are 4096 4k slabs
 * then only 12 bits are needed for the index.  and note that a pool
 * need not be used exclusively for the same type of element;
 * the indexes can be scattered about ...
 *
 * also, use 0 for error, the first index (zeroth element) is 1
 */

// example: 
u16 inline
mem_addr_to_u16(struct mem_pool * pool, u64 addr)
{
    if (addr < pool->base  ||  addr >= pool->base + pool->size) {
        printf("mem_addr_to_u16 error: 0x%lx is not within pool %s\n", addr, pool->name);
        stkdump();
    }
    //u16 x = (addr - pool->base) / pool->usize;
    //printf("+++ mem_addr_to_u16: a(%lx) b(%lx) u(%d) -> 0x%x (0x%x)\n",
    //            addr, pool->base, pool->usize, (addr - pool->base) / pool->usize, x);

    int x = (addr - pool->base) / pool->usize;
    if (x > 0xfffe) {
        printf("mem_addr_to_u16 error: u16 overflow (%x)\n", x);
        stkdump();
    }
    return x + 1;
}

u64 inline
mem_u16_to_addr(struct mem_pool * pool, u16 index)
{
    if (index == 0) {
        printf("mem_u16_to_addr error: looks like you had an alloc error a while back,\n");
        printf("                       you really need to check your return codes!\n");
        return 0;
    }

    index--;

    if (index > pool->size / pool->usize) {
        printf("mem_u16_to_addr error: index %d is not within pool %s\n", index, pool->name);
        stkdump();
        return 0;
    }

    return index * pool->usize + pool->base;
}

