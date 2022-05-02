
/* ---- bitmap functions
 * needed for slab memory management among other things
 */

#include "uos.h"

void
bitmap_create(struct bitmap * map, int size)
{
    map->nblks = ROUNDUP_SHR(size, BLK_BITS);
    map->count = 0;
    memset(map->blks, 0, map->nblks * sizeof(BLK_TYPE));
    return;
}

void
bitmap_set(struct bitmap * map, int n)
{
    int blk = n / BLK_BITS;
    int bit = n % BLK_BITS;
    if ((map->blks[blk] & (1UL << bit)) == 0) {
        map->blks[blk] |= (1UL << bit);
        map->count++;
    }
}

int
bitmap_clear(struct bitmap * map, int n)
{
    int blk = n / BLK_BITS;
    int bit = n % BLK_BITS;
    if (map->blks[blk] & (1UL << bit)) {
        map->blks[blk] &= ~(1UL << bit);
        map->count--;
        return 0;
    }
    return 1;
}

/*
 * find the first free object index,
 * -1 means the bitmap is full
 */
int
bitmap_first_1_free(struct bitmap * map)
{
    int blk;
    int bit;

    for (blk = 0; blk < map->nblks; blk++)
        if (map->blks[blk] != ALL_BITS_ON)
            for (bit = 0; bit < BLK_BITS; bit++)
                if ((map->blks[blk] & (1UL << bit)) == 0)
                    return blk * BLK_BITS + bit;
    return -1;
}

/*
 * find the index of the first N free objects,
 * -1 means no N contiguous objects
 */
int
bitmap_first_n_free(struct bitmap * map, int n)
{
    int blk;
    int bit;
    int i = 0;
    int idx = -1;

    for (blk = 0; blk < map->nblks; blk++)
        if (map->blks[blk] != ALL_BITS_ON)
            for (bit = 0; bit < BLK_BITS; bit++)
                if ((map->blks[blk] & (1UL << bit)) == 0) {
                    if (idx == -1)
                        idx = blk * BLK_BITS + bit;
                    i++;
                    if (i >= n)
                        return idx;
                } else {
                    idx = -1;
                    i = 0;
                }
    return -1;
}

