/*
 * dtb.c - extract useful config values from a dtb tree
 *
 * https://devicetree-specification.readthedocs.io/en/stable/flattened-format.html
 */

#include "uos.h"
#include <stdarg.h>

void printi(int depth, const char * fmt, ...);
int  printf(const char * fmt, ...);
int  strcmp(const char * s1, const char * s2);
long strlen(const char * s);

#define DTB_MAGIC       0xd00dfeed
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

#define BYTE(x, n)      ((u64)((u8 *)&x)[n])

// big endian, bah
#define FDT16(x) ((BYTE(x, 0) <<  8) |  BYTE(x, 1))
#define FDT32(x) ((BYTE(x, 0) << 24) | (BYTE(x, 1) << 16) | (BYTE(x, 2) <<  8) |  BYTE(x, 3))
#define FDT64(x) ((BYTE(x, 0) << 56) | (BYTE(x, 1) << 48) | (BYTE(x, 2) << 40) | (BYTE(x, 3) << 32) | \
                  (BYTE(x, 4) << 24) | (BYTE(x, 5) << 16) | (BYTE(x, 6) <<  8) |  BYTE(x, 7))

static inline u16 fdt16(u16 x) { return (u16)FDT16(x); }
static inline u32 fdt32(u32 x) { return (u32)FDT32(x); }
static inline u64 fdt64(u64 x) { return (u64)FDT64(x); }
//static inline u16 cpu_to_fdt16(u16 x) { return (u16)FDT16(x); }
//static inline u32 cpu_to_fdt32(u32 x) { return (u32)FDT32(x); }
//static inline u64 cpu_to_fdt64(u64 x) { return (u64)FDT64(x); }

struct dtb_hdr {
    u32 magic;
    u32 size;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
} __attribute__((packed));

struct dtb_prop {
    u32 len;
    u32 noff;
};


int
is_dtb(struct dtb_hdr * dtb)
{
    return fdt32(dtb->magic) == DTB_MAGIC;
}

static char *
is_dstr(u32 * q, int len)
{
    char * p = (char *)q;
    // try to detect if this is a string or not  :D
    if (p[len-1] != '\0') return 0;
    while (*p != 0) {
        if (*p < 0x20 || *p > 0x7f)
            return 0;
        p++;
    }
    return (char *)q;
}

static int
startswith(char * path, char * test)
{
    if (*test == '\0') return 0;
    //printf("++ %s / %s\n", path, test);
    while (*path == *test) {
        path++;
        test++;
        if (*path == '*')               // wildcard - if we get this far, its a match
            return 1;
    }
    return (*test == '\0');
}

// return the string after the first char c
static char *
after(char *path, char c)
{
    char * p = path;
    while (*p != c  &&  *p != '\0')
        p++;
    return (*p == c) ? p+1 : 0;
}

// 'path' must contain a node and a property separated with a colon.
// scan nodes linearly, call callback with the property's value.
// caller must understand the property's type and handle it correctly.
void
dtb_lookup(struct dtb_hdr * dtb, char * path, void (* callback)(char *, u64))
{
    char * strings = (char *)((u64)dtb + fdt32(dtb->off_dt_strings));
    u32  * q = (u32 *)((u64)dtb + fdt32(dtb->off_dt_struct));
    char * search_prop = 0;
    u64    search_val = 0;
    char * node;
    int    depth = 0;
    int    n;

    do {
        switch (fdt32(*q)) {
        case FDT_BEGIN_NODE:
            depth++;
            q++;
            node = (char *)q;
            //printi(depth, "node: %s\n", node);
            n = strlen((char *)q);
            if (startswith(path, (char *)q))
                search_prop = after(path, ':');
            q += 1 + n/4;

            while (fdt32(*q) == FDT_PROP) {
                struct dtb_prop * prop = (struct dtb_prop *)++q;
                int len = fdt32(prop->len);
                search_val = 0;
                if (strcmp(search_prop, strings + fdt32(prop->noff)) == 0) {
                    //printi(depth, "  prop: %s\n", strings + fdt32(prop->noff));
                    search_val = (u64)q;
                }
                q += sizeof(struct dtb_prop)/sizeof(*q);
                if (len > 0) {
                    char * s = is_dstr(q, len);
                    if (search_val) {
                        //printf("+++ %x %s\n", q, s);
                        if (s)
                            callback(node, (u64)q);
                        else
                            callback(node, (u64)fdt32(*q));
                    }
                    q += 1 + (len-1)/4;
                }
            }
            search_prop = 0;
            break;

        case FDT_END_NODE:
            depth--;
            q++;
            break;

        default:
            printi(depth, " what is %x?\n", fdt32(*q));
            q++;
        }
    } while (depth > 0  &&  (u64)q < ((u64)dtb + fdt32(dtb->size_dt_struct)));
}

// print all the nodes
void
dtb_dump(struct dtb_hdr * dtb)
{
    int depth = 0;

    printf("dtb: magic      %x\n", fdt32(dtb->magic));
    printf("dtb: version    %x\n", fdt32(dtb->version));
    printf("dtb: size       %x\n", fdt32(dtb->size));
    printf("dtb: dt struct  %x\n", fdt32(dtb->off_dt_struct));
    printf("dtb: dt size    %x\n", fdt32(dtb->size_dt_struct));
    printf("dtb: dt strings %x\n", fdt32(dtb->off_dt_strings));

    char * strings = (char *)((u64)dtb + fdt32(dtb->off_dt_strings));
    u32  * q = (u32 *)((u64)dtb + fdt32(dtb->off_dt_struct));

    do {
        switch (fdt32(*q)) {
        case FDT_BEGIN_NODE:
            depth++;
            q++;
            printi(depth, "node: %s\n", (char *)q);
            q += 1 + strlen((char *)q)/4;

            while (fdt32(*q) == FDT_PROP) {
                struct dtb_prop * prop = (struct dtb_prop *)++q;
                int len = fdt32(prop->len);
                //printi(depth, "  prop: %s (%x)", strings + fdt32(prop->noff), len);
                printi(depth, "  prop: %s", strings + fdt32(prop->noff));
                q += sizeof(struct dtb_prop)/sizeof(*q);
                if (len > 0) {
                    char * s = is_dstr(q, len);
                    if (s) {
                        printf(" %s\n", s);
                        q += 1 + (len-1)/4;
                    } else {
                        switch (len) {
                        case 2:  printf(" %x\n", fdt16(*q++)); break;
                        case 4:  printf(" %x\n", fdt32(*q++)); break;
                        case 8:  printf(" %x\n", fdt64(*q++)); q++; break;
                        default:
                            int i;
                            for (i = 0; i < len; i += 4) {
                                if (i > 0  &&  (i % 16) ==  0) printf("                        ");
                                printf(" %x", q++);
                                if ((i % 16) == 12) printf("\n");
                            }
                            if (i < 16  ||  (i % 16) != 0) printf("\n");
                        }
                    }
                } else {
                    printf("\n");
                }
            }
            break;

        case FDT_END_NODE:
            depth--;
            q++;
            break;

        default:
            printi(depth, " what is %x?\n", fdt32(*q));
            q++;
        }
    } while (depth > 0  &&  (u64)q < ((u64)dtb + fdt32(dtb->size_dt_struct)));
}
