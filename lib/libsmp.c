
#include "uos.h"

static int smp_next_cpu = 1;    // 0 is the boot cpu

void
smp_newcpu(struct thread * th)
{
    u64 x;
    //__asm__ volatile("mrs %0, sctlr_el2" : "=r" (x) : : );
    //__asm__ volatile("mrs %0, S3_1_c15_c2_0" : "=r" (y));
    //__asm__ volatile("mrs %0, S3_1_c15_c2_1" : "=r" (z));
    //__asm__ volatile("mrs %0, mair_el1" : "=r" (x));
    //printf("    mair_el1(0x%lx)\n", x);
    //__asm__ volatile("mrs %0, amair_el1" : "=r" (x));
    //printf("   amair_el1(0x%lx)\n", x);
    //__asm__ volatile("mrs %0, ttbr0_el1" : "=r" (x));
    //printf("   ttbr0_el1(0x%lx)\n", x);
    //__asm__ volatile("mrs %0, ttbr1_el1" : "=r" (x));
    //printf("   ttbr1_el1(0x%lx)\n", x);
    //__asm__ volatile("mrs %0, tcr_el1" : "=r" (x));
    //printf("     tcr_el1(0x%lx)\n", x);

    printf("smp%d/%d: el%d tls(%x) sp(%x)   sctlr_el2(0x%lx)\n", th->thno, _cpu_id(),
                      get_exec_level(), get_tp(), get_sp(), x);

    th->func(th);

    printf("cpu(%d) exiting\n", _cpu_id());
};

int
smp_start_thread(void (* func)(struct thread *), void * arg0)
{
    int cpu = smp_next_cpu++;

    struct thread * th = (struct thread *)mem_zalloc(pool4k, 1);
    if (th == 0) {
        printf("pool4k empty - cannot start thread\n");
        return -1;
    }

    th->sp   = (u64)th + 0x1000 - STACK_EXTRA;
    th->thno = cpu;
    th->func = func;
    th->arg0 = arg0;
    th->tsk  = 0;

    //printf("+++ smp_start_thread: cpu(%d) sp(0x%x) fn(0x%x) arg(0x%x)\n",
    //                                            cpu, th->sp, func, arg0);

    int rc;
    // TODO move to arch.h
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

    if (rc)
        printf("*** smp_start_thread error: 0x%x\n", rc);
    return rc;
}

static void
thd(struct thread * th)
{
    printf("thread %d: cpu(%d) arg(%x)\n", th->thno, cpu_id(), th->arg0);

    // yielding with a null task context puts
    // the cpu into general circulation
    //yield();
    while (1) pause();

    printf("thd: should never get here ...\n");
}

int
smp_init(int max_cpus)
{
    if (max_cpus == -1)
        max_cpus = MAX_CPUS;

    // first time here: fix up the boot thread
    if (smp_next_cpu == 1) {
        printf("smp0/%d: el%d tls(%x) sp(%x)\n", _cpu_id(), get_exec_level(),
                                    get_tp(), get_sp());
        struct thread * th = get_thread_data();
        th->sp   = get_sp();
        th->thno = 0;
    }

    //int n = 1;      // assume cpu 0 is running
    int n = 0;
    //while (smp_start_thread(thd, (void *)(u64)n) == 0)
    while (smp_start_thread(thd, (void *)0) == 0)
        if (++n >= max_cpus)
            break;

    printf("smp_init added %d cpus\n", n);

    return n;
}
