
#include "uos.h"

#define calibrate_time

/* ---- mem tests ----------------------------------------------- */
/*
 * test_mem_01 - test the mem slab allocator (mainly the bitmaps)
 */

int
test_mem_01(void)
{
    struct bitmap * map = &pool128k->map;
    int max = pool128k->size / pool128k->usize;
    u64 base = 0;

    if (map->blks[0] != 0)
        printf("*** WARNING: previous allocations from pool128k exist ***\n");

    if (max > 64)
        max = 64;

    u64 x = 1;
    printf("  test_mem_01 %016lx %8lx %5d\n", map->blks[0], x, map->count);
    int i;
    for (i = 0; (i < max)  &&  x; i++) {
        x = mem_alloc(pool128k, 1);
        if (base == 0)
            base = x;
        printf("  test_mem_01 %016lx %8lx %5d\n", map->blks[i / 64], x, map->count);
    }

    // ~random frees, generate some 10 block holes
    int k = 2;
    for (int j = 0; j < i; j += k) {
        mem_free(pool128k, base + j * 128 * 1024, k);
        printf("  test_mem_01 %016lx %5ld\n", map->blks[j / 64], map->count);
        k = j / 8 + 2;
    }

    u64 y = mem_alloc(pool128k, 10);
    printf("  test_mem_01 %016lx %8lx %5d\n", map->blks[0], y, map->count);
    x = mem_alloc(pool128k, 10);
    printf("  test_mem_01 %016lx %8lx %5d\n", map->blks[0], x, map->count);
    x = mem_alloc(pool128k, 10);
    printf("  test_mem_01 %016lx %8lx %5d\n", map->blks[0], x, map->count);

    // FIXME if max != 64 blk_rslt will be different ...
    u64 blk_rslt = 0xff8081ffe01007ff;
    if (map->blks[0] != blk_rslt) {
        printf("  test_mem_01 failed: %016lx != %016lx\n", map->blks[0], blk_rslt);
        return 1;
    }

    // FIXME interesting ... the loop below, which frees individual blocks no matter
    // if they were allocated in a group or not (a "feature" which is arguably not
    // a good idea) does not work if the address is within a group allocation where
    // the group's base address == the pool's base address ... need to debug
    mem_zfree(pool128k, y, 10);

    // free everything in pool128k
    int err = 0;
    for (i = 0; i < 64; i++) {
        if (blk_rslt & ((u64)1 << i)) {
            printf("  test_mem_01 %016lx %5d\n", map->blks[0], map->count); // debug rpi400
            err += mem_zfree(pool128k, base + i * 128 * 1024, 1);
            printf("  test_mem_01 %016lx %5d %d\n", map->blks[0], map->count, err);
        }
    }

    if (map->blks[0] != 0) {
        printf("  test_mem_01 failed: %016lx != 0\n", map->blks[0]);
        return 1;
    }

    return err;
}


#define MODULO  795028841
#define STEP    360287471
u64     Rnd_start = 0;          // optionally some starting value like 433494437

static u64
prand(void)
{
    u64 next = (Rnd_start + STEP) % MODULO;
    Rnd_start = next;
    return next;
}

/*
 * test_mem_02 - test the cpu mem write speed (cache settings - see boot.s)
 */

int
test_mem_02(void)
{
    u64 start = etime();
    u64 end;
    u64 * lst = (u64 *)mem_alloc(pool128k, 1);
    // one 128k block can hold 128 * 1024 / sizeof(u64 *) ==> 16k pointers
    // but pool4k only has 8k, and we need a few for other things ...
    int max   = 8000;
    int n     = 0;

    while (n < max)
        if ((lst[n++] = mem_alloc(pool4k, 1)) == 0) break;
    printf("max(%d) n(%d)\n", max, n);

    for (int j = 0; j < 4; j++) {
        u64 r = prand();
        for (int i = 0; i < n; i++)
            memset((void *)lst[i], r, 4096);

        end = etime();
        printf("etime(%d)\n", end - start);
        start = end;
    }

    for (int i = 0; i < n; i++) {
        //printf("%4d 0x%08x %9d\n", i, lst[i], *(u64 *)lst[i]);
        mem_free(pool4k, lst[i], 1);
    }
    mem_free(pool128k, (u64)lst, 1);

    return 0;
}

/* ---- test main ----------------------------------------------- */

int
test(int suite)
{
    printf(" ---- test 0x%x ------------------------------------\n", suite);

    if (suite == 0) {
#       ifdef calibrate_time
        // to calibrate:
        // - eyeball the start wall clock time and this count,
        // - wait a few hours/days,
        // - eyeball the end wall clock time and count,
        // - do the math.
        u64 clock = etime() + 1000000;
        printf("start %lx\n", clock);
        while (1) {
            if (etime() > clock) {
                clock += 1000000 ;
                printf("clock %lx\n", clock);
            }
        }
#       endif

    } else if ((suite & 0xf0) == 0x10) {
        switch (suite & 0xf) {
        case 0x1:  if (test_mem_01()) return 1;  break;
        case 0x2:  if (test_mem_02()) return 1;  break;
        default:   return 1;
        }
        return 0;
    }

    printf("unknown test 0x%x\n", suite);
    return 1;
}
