
void    stkdump(void);
int     stkcheck(void);

// FIXME gcc prologue for first task func writes above stack starting point!
// as a hack, move the stack starting position further down,
// and this might not be enough ... !!  (wish we could tell gcc to stop)
#define STACK_EXTRA     0x80

/* Get the execution priviledge level
 *
 * We're rolling with the ARM values, not x86.
 * Level 0 is user mode
 * Level 1 is the kernel
 * Level 2 is a hypervisor
 * Level 3 is a secure manager
 */
static inline u32 get_exec_level(void)
{
    u32 status;
    __asm__("mrs %0, currentel; lsr %0, %0, 2" : "=r" (status) : : );
    return status;
}

// for now, tls is simply used as a sprintf() buffer
static inline char * get_tp(void)
{
    char * tp;
    asm volatile("mrs %0, tpidr_el0" : "=r" (tp) : : );
    return tp;
}

static inline struct thread *
get_thread_data(void)
{
    return (struct thread *)get_tp();
}

// only works after tp has been established (in smp_init)
static inline int
cpu_id(void)
{
    struct thread * th = (struct thread *)get_tp();
    return th->thno;
}

// hardware version ...
static inline int
_cpu_id(void)
{
    u64 id;
    asm volatile("mrs %0, mpidr_el1" : "=r" (id) : : );
    return id & 0xff;
}

static inline u64
get_sp(void)
{
    u64 sp;
    asm volatile("mov %0, sp"  : "=r" (sp) : );
    return sp;
}

#define pause(x)    asm("wfe")
#define resume(x)   asm("sev")    //asm("dsb ishst; sev")

// time delay (uncalibrated - n has no units)
#define delay(n)    for (volatile int x = 0; x < (n) * 1000; x++)

static inline void dc_flush(u64 va)
{
    asm volatile("dc cvac, %0\n\t" : : "r" (va) : "memory");
}

static inline void dc_invalidate(u64 va)
{
    asm volatile("dc civac, %0\n\t" : : "r" (va) : "memory");
}

// for testing the exceptioin handler
static inline void cause_exc(void)
{
    int x = 0;
    asm ("msr  vbar_el3, %0" : : "r" (x));
}

// call the supervisor (a kernel, hypervisor, or secure monitor)
// to enable a cpu core
static inline int
cpu_enable(int cpu, struct thread * th)
{
    int rc = 0;
#   ifdef HVC
    // http://wiki.baylibre.com/doku.php?id=psci
    //   psci {
    //          migrate = < 0xc4000005 >;
    //          cpu_on = < 0xc4000003 >;
    //          cpu_off = < 0x84000002 >;
    //          cpu_suspend = < 0xc4000001 >;
    //          method = "hvc";
    //          compatible = "arm,psci-0.2\0arm,psci";
    //   };
    //
    // power on cpu N, point it at the smp_entry boot code,
    // and use the 'th' pointer as 'context'
    asm volatile (
        "ldr  x0, =0xc4000003   \n" // cpu on
        "mov  x1, %1            \n" // cpu number
        "ldr  x2, =smp_entry    \n" // func addr
        "mov  x3, %2            \n" // context id (th)
        "hvc  0                 \n" // hypervisor fn 0
        "mov  %0, x0            \n"
    : "=r" (rc) : "r" (cpu), "r" (th) : "x0","x1","x2","x3");
#   endif

#   ifdef SMC
    //  psci {
    //          compatible = "arm,psci-0.2";
    //          method = "smc";
    //  };
    asm volatile (
        "ldr  x0, =0xc4000003   \n" // cpu on
        "mov  x1, %1            \n" // cpu number
        "ldr  x2, =smp_entry    \n" // func addr
        "mov  x3, %2            \n" // context id (th)
        "smc  0                 \n" // secure monitor fn 0
        "mov  %0, x0            \n"
    : "=r" (rc) : "r" (cpu), "r" (th) : "x0","x1","x2","x3");
#   endif

    return rc;
}

