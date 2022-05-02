
#define BLK_TYPE        u64
#define ALL_BITS_ON     ((BLK_TYPE)-1)
#define BLK_BITS        (sizeof(BLK_TYPE) * BITS_PER_BYTE)

struct bitmap {
    int nblks;
    int count;
    u64 blks[0];
};

void  bitmap_create(struct bitmap * map, int size);
void  bitmap_set(struct bitmap * map, int n);
int   bitmap_clear(struct bitmap * map, int n);
int   bitmap_first_free(struct bitmap * map);
int   bitmap_first_n_free(struct bitmap * map, int n);

