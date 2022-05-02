
/*
 * Define the uos memory layout
 *
 * The MMU is not yet turned on, so the logical address map
 * equals physical.  RAM unused by uos is carved (manually) into
 * slabs of uniform pages. You decide which sizes and how many of
 * each are needed. Eventually tools that analyze the app usage
 * will let us tune this - for now make a shot from the hip.
 *
 * This version is for QEMU with the -m default (128M)
 * The first 1MB is reserved for code + data + bss
 * The arch+mach .ld file sets the location of _free_mem
 *  - create a  3MB region for 12288x  256 byte pages
 *  - create a 32MB region for  8192x   4k pages
 *  - create the rest as         736x 128k pages
 */

#include "uos.h"

struct mem_pool * pool256b = 0;
struct mem_pool * pool4k   = 0;
struct mem_pool * pool128k = 0;

void
mem_init(void)
{
    // start at _free_mem rounded up to the next 1MB boundary
    u64 base = ROUNDUP((u64)&_free_mem, 1024 * 1024);

#   ifdef DEBUG
    printf("+++ struct bitmap   size %d\n", sizeof(struct bitmap));
    printf("+++ struct mem_pool size %d\n", sizeof(struct mem_pool));
#   ifdef CACHE
    printf("+++ mem_alloc cache size %d\n", numof(pool4k->cache));
#   endif
    printf("initializing RAM ...\n");
#   endif

    // create a 3MB 256b pool behind the code
#   define SIZE256b (      12288 * 256)
    pool256b = mem_pool_create("256b", base, SIZE256b, 256, 0);
    if (pool256b == 0)  return;
    base += SIZE256b;

#   define SIZE4k   (  8192 * 4 * 1024)             // to 0x41000000
    pool4k = mem_pool_create("4k", base, SIZE4k, 4 * 1024, 0);
    if (pool4k == 0)  return;
    base += SIZE4k;
    // if there is a spare core, background init the rest of the 4k pool
#   ifdef ZERO_4K
    create_task("zero_4k", mem_pool_zero, pool4k, 0);
#   endif

#   define SIZE128k ( 736 * 128 * 1024)             // to 0x50000000
    pool128k = mem_pool_create("128k", base, SIZE128k, 128 * 1024, 0);
}

