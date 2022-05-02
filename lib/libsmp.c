
#include "uos.h"

static int smp_next_cpu = 1;    // 0 is the boot cpu

void
smp_newcpu(struct thread * th)
{
    printf("smp%d/%d: el%d tls(%x) sp(%x)\n", th->thno, _cpu_id(),
                      get_exec_level(), get_tp(), get_sp());

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

    int rc = cpu_enable(cpu, th);
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
    yield();

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
