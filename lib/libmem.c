
/* -------- slab memory management
 * imo the ONLY proper way to manage linear address space
 */

#include "uos.h"

#define CACHE           // a few of the last freed pages

#define numof(x)        (sizeof(x)/sizeof((x)[0]))

// ugh, I hate global locks, but until I rewrite gate.h
// to allow placing a gate struct inside another struct ...
/*static*/ SPMC_GATE_INIT(pool, MAX_CPUS);

/*
 * the caller must remember what N was ...
 */
u64
mem_alloc(struct mem_pool * pool, int n)
{
    u64 addr = 0;

    if (n <= 0) {
        printf("mem_alloc error: bogus number of elements (%d)\n", n);
        return 0;
    }

    if (pool == 0) {
        printf("mem_alloc error: pool uninitialized\n");
        stkdump();
        return 0;
    }

#   ifdef CACHE
    // FIXME race on cstk ... maybe x=atomic_dec_fetch(); if (x >= 0) ... else atomic_inc()
    if (n == 1  &&  pool->cstk > 0)
        return mem_u16_to_addr(pool, pool->cache[atomic_dec_fetch(pool->cstk)]);
#   endif

#   if MAX_CPUS > 1
    u64 token;
    wait_for_token(pool, token);
#   endif
    {
        int idx = bitmap_first_n_free(&pool->map, n);
        if (idx == -1) {
#           if MAX_CPUS > 1
            return_token(pool, token);
#           endif
            printf("mem_alloc error: pool map %s is full\n", pool->name);
            stkdump();
            return 0;
        }

        addr = idx * pool->usize + pool->base;
        // bitmap_first_n_free can run off the end, double check the addr
        if (addr > pool->base + pool->size - pool->usize) {
#           if MAX_CPUS > 1
            return_token(pool, token);
#           endif
            printf("mem_alloc error: pool %s is empty\n", pool->name);
            //stkdump();
            return 0;
        }

        for (int i = 0; i < n; i++)
            bitmap_set(&pool->map, idx + i);
    }
#   if MAX_CPUS > 1
    return_token(pool, token);
#   endif

    //printf("[[a 0x%08lx x%d %s]]\n", addr, n, pool->name);
    return addr;
}

u64
mem_zalloc(struct mem_pool * pool, int n)
{
    u64 m = mem_alloc(pool, n);
    if (m) memclr((void *)m, n * pool->usize);
    return m;
}

int
mem_is_in(struct mem_pool * pool, u64 addr)
{
    if (addr < pool->base  ||  addr >= pool->base + pool->size)
        return 0;
    if (addr % pool->usize)
        return 0;
    return 1;
}

int
mem_free(struct mem_pool * pool, u64 addr, int n)
{

    if (n <= 0) {
        printf("mem_free error: bogus number of elements (%d)\n", n);
        return 0;
    }

    if (!mem_is_in(pool, addr)) {
        printf("mem_free error: 0x%lx is not within pool %s\n", addr, pool->name);
        stkdump();
        return 1;
    }

    // FIXME  Yes, caching recent freed block is fast, but the
    // double-free error catch is missing!  Gotta mimally scan that
    // cache list before saying OK ... but not if there is only one
    // thread using mem_*() functions  :*

#   ifdef CACHE
    // room in the cache?  if so, don't free the block, just hold it.
    // FIXME room for 2 is a sloppy way to manage the race between
    // two threads freeing a block in the same pool at the same time
    // ... but the whole point of the per-pool cache is to skip the
    // global pool lock   (see 'maybe' above ...)
    //if (n == 1  &&  pool->cstk + 2 <= numof(pool->cache))
    if ((n == 1)  &&  ((pool->cstk) + 2 <= numof(pool->cache))) {
        pool->cache[atomic_fetch_inc(pool->cstk)] = mem_addr_to_u16(pool, addr);
        return 0;
    }
#   endif

    int err = 0;
    int idx = (addr - pool->base) / pool->usize;
    u64 token;
#   if MAX_CPUS > 1
    wait_for_token(pool, token);
#   endif
    {
        for (int i = 0; i < n; i++)
            if (bitmap_clear(&pool->map, idx + i)) {
                printf("mem_free error: 0x%lx is already free in pool %s\n",
                        addr + i * pool->usize, pool->name);
                err++;
            }
    }
#   if MAX_CPUS > 0
    return_token(pool, token);
#   endif

    //printf("[[f 0x%08lx x%d %s]]\n", addr, n, pool->name);
    //if (err) stkdump();
    return err;
}

int
mem_zfree(struct mem_pool * pool, u64 addr, int n)
{
    if (!mem_is_in(pool, addr)) {
        printf("mem_free error: 0x%lx is not within pool %s\n", addr, pool->name);
        stkdump();
        return 1;
    }

    memclr((void *)addr, n * pool->usize);

    return mem_free(pool, addr, n);
}


/*
 * mem_pool_create()
 *
 *  name  - for error and info messages
 *  base  - physical start of new region
 *  size  - size of the region; will be split into mgt structs, and blocks
 *          (note: the resulting available pool size will be smaller)
 *  usize - unit size of the blocks in this region
 *  init  - zero all blocks
 *
 *  returns 0 on error, else a pointer to be used for
 *  subsequent mam_alloc() and mem_free() functions.
 */
struct mem_pool *
mem_pool_create(char * name, u64 base, u64 size, u64 usize, int zero)
{
    // the first block(s) are reserved for the
    // pool data structure and bitmap

    struct mem_pool * pool = (struct mem_pool *)base;

    int err = 0;
    if (n_bits_set(usize) != 1) {
        printf("mem_pool_create error %s: usize must be a power of 2 (%d)\n", name, usize);
        stkdump();
        err++;
    }
    if (size < usize * 2) {
        printf("mem_pool_create error %s: size (%ld) must mimally be 2x unit size (only %ld)\n",
                name, size, usize);
        stkdump();
        err++;
    }
    if ((size % usize) != 0) {
        printf("mem_pool_create error %s: size must be a multiple of usize\n", name);
        stkdump();
        err++;
    }
    if (err)
        return 0;

    // estimate nblks
    int eblks = ROUNDUP_SHR(size, usize);
    // actual nblks
    int nblks = (size - sizeof(struct mem_pool)
                      - ROUNDUP_SHR(eblks, BITS_PER_BYTE)
                 ) / usize;
    //printf("+++ base(0x%x) size(0x%x) eblks(%d) nblks(%d) rblks(%d)\n",
    //                base, size, eblks, nblks, eblks - nblks);

    pool->base  = base + usize * (eblks - nblks);
    pool->size  = size - usize * (eblks - nblks);
    pool->name  = name;
    pool->usize = usize;
    pool->cstk  = 0;

    memset((void *)(pool->map.blks), 0, nblks / BLK_BITS);
    if (zero)
        memclr((void *)pool->base, pool->size);

    bitmap_create(&pool->map, nblks);
    printf("mem_pool %-8s at 0x%lx end 0x%lx   %d units (mgt %db)\n", name, base, base + size, nblks,
           sizeof(struct mem_pool) + ROUNDUP_SHR(eblks, BITS_PER_BYTE));
    return pool;
}


#if 0
void
mem_pool_debug(struct mem_pool * pool)
{
    if (!pool) {
        printf("mem_pool_debug: pool is null\n");
        return;
    }

    printf("pool %s: base(0x%x) size(0x%x) usize(%d) nblks(%d)\n",
            pool->name, pool->base, pool->size, pool->usize, pool->map.nblks);
    for (int i = 0; i < pool->map.nblks; i++)
        printf("    %016lx\n", pool->map.blks[i]);
}
#endif

// a task for backgrounding memory initialization
void
mem_pool_zero(void * arg0)
{
    struct mem_pool * pool = (struct mem_pool *)arg0;
    // mitigate the race condition created by running mem_pool_zero()
    // as a task by starting the zero from the next free unit ...
    // (mem_alloc the first block to find the start address)
    u64 mem = mem_alloc(pool, 1);
    memclr((void *)mem, pool->size - (mem - pool->base));
    mem_zfree(pool, mem, 1);
}

