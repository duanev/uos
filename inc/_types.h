
// standard typedefs and basic binary math macros

#define BITS_PER_BYTE       8

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef char                i8;
typedef short               i16;
typedef int                 i32;
typedef long long           i64;

#define LEN(a)              (sizeof(a) / sizeof(a[0]))      // for statically declared arrays

#define ROUNDUP(s, u)       (((s) + (u) - 1) / (u) * (u))   // round integers up to units of 'u'
#define ROUNDUP_SHR(s, u)   (((s) + (u) - 1) / (u))         // round up but leave shifted right by units

// minimum non-zero, max and min
#define minnz(a, b) ((a) == 0 ? (b) : (b) == 0 || (a) < (b) ? (a) : (b))
#define max(a, b)   ((a) > (b) ? (a) : (b))
#define min(a, b)   ((a) < (b) ? (a) : (b))

// create a base 2 LOG(n) macro (works up to 2^31)
#define LOG_1(n) (((n) >= 2)     ? 1 : 0)
#define LOG_2(n) (((n) >= 1<<2)  ? (2 + LOG_1((n)>>2)) : LOG_1(n))
#define LOG_4(n) (((n) >= 1<<4)  ? (4 + LOG_2((n)>>4)) : LOG_2(n))
#define LOG_8(n) (((n) >= 1<<8)  ? (8 + LOG_4((n)>>8)) : LOG_4(n))
#define LOG(n)   (((n) >= 1<<16) ? (16 + LOG_8((n)>>16)) : LOG_8(n))

