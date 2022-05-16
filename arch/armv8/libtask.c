
#include "uos.h"


struct task {
    u64             tid;
    u64             flags;
    char          * name;
    u64             _res0;

    taskfn          func;
    volatile void * arg;                        // volatile in case data is returned

    u64             sp;
    u64             x0;

    // all the callee-saved regs
    // https://modexp.wordpress.com/2018/10/30/arm64-assembly
    u64             x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, fp, lr;

    // ensure the registers above remain in order
#   define  X19_OFF __builtin_offsetof(struct task, x19)

    // keep struct size a multiple of 64 (currently 192)

    u64             _res1, _res2, _res3, _res4;
};

static u64 next_tid = 1;


#define RUNQ_MAX    (MAX_CPUS * 16)
#define RUNQ_HIWAT  (RUNQ_MAX - MAX_CPUS)       // guarantee room for running threads

MPMC_GATE_INIT(runq, RUNQ_MAX);


inline char *
get_task_name(void)
{
    struct thread * th = get_thread_data();
    return th->tsk->name;
}

inline u64
get_task_id(void)
{
    struct thread * th = get_thread_data();
    return th->tsk->tid;
}

#if 0
static char *
task_dbg(u64 tptr)
{
    struct thread * th = get_thread_data();
    struct task * tsk = (struct task *)tptr;
    if (tptr)
        sprintf(th->scratch, "%s", tsk->name);
    else
        th->scratch[0] = 0;
    return th->scratch;
}

// call this from watchdog
// but race conditions are likely ...
int
task_debug(void)
{
    if (runq_gate.len)
        debug_mpmc_gate(runq, task_dbg);
    return runq_gate.len;
}
#endif

static inline void
armv8_set_sp(u64 sp)
{
    asm volatile("mov sp, %0" : : "r" (sp) : );
}

void inline
save_task_context(struct task * tsk)
{
    asm ("stp x19, x20, [%0, %c[x19o]]\n"
         "stp x21, x22, [%0, %c[x19o] + 0x10]\n"
         "stp x23, x24, [%0, %c[x19o] + 0x20]\n"
         "stp x25, x26, [%0, %c[x19o] + 0x30]\n"
         "stp x27, x28, [%0, %c[x19o] + 0x40]\n"
         "stp fp,  lr,  [%0, %c[x19o] + 0x50]\n" : "=r" (tsk) : [x19o] "i" (X19_OFF) );

    asm volatile("mov %0, sp"  : "=r" (tsk->sp) : );
}

void inline
restore_task_context(struct task * tsk)
{
    asm ("ldp x19, x20, [%0, %c[x19o]]\n"
         "ldp x21, x22, [%0, %c[x19o] + 0x10]\n"
         "ldp x23, x24, [%0, %c[x19o] + 0x20]\n"
         "ldp x25, x26, [%0, %c[x19o] + 0x30]\n"
         "ldp x27, x28, [%0, %c[x19o] + 0x40]\n"
         "ldp fp,  lr,  [%0, %c[x19o] + 0x50]\n" : "=r" (tsk) : [x19o] "i" (X19_OFF) );

    asm volatile("mov sp,  %0" : : "r" (tsk->sp) : );
    // gcc needs x0 loaded last (go look at the disassembly)
    asm volatile("mov x0,  %0" : : "r" (tsk->x0) : );
}

// noinline forces a consistent stack frame
void __attribute__ ((noinline))
yield(void)
{
    volatile struct thread * th = get_thread_data();
    //struct thread * th = get_thread_data();

    if (th->tsk) {
        // if this thread is a running task, save the restore point
        save_task_context(th->tsk);
        add_item(runq, th->tsk);
        th->tsk = 0;
    }

    // get the next task to run (and wait if needed)

    remove_item_wait(runq, th->tsk, struct task *);

    // restore the selected task

    restore_task_context(th->tsk);
}

// yield to other pending tasks for at least msecs miliseconds.
// non-deterministic (not real-time) as we don't know when or if
// another task will yield back to us.
void __attribute__ ((noinline))
yield_for(int msecs)
{
    u64 end = (u64)msecs * 1000 + etime();

    while (end > etime()) {
        yield();
        //printf("  %10d > %10d\n", end, etime());      // debug
    }
}


static void
_start_task(struct task * tsk)
{
    //printf("task %ld [%s] starting   [arg=0x%x,tsk=0x%x]\n", tsk->tid, tsk->name, tsk->arg, tsk);
    printf("task %6ld [%s] starting\n", tsk->tid, tsk->name);

    tsk->func(tsk->arg);

    printf("task %6ld [%s] ending\n", tsk->tid, tsk->name);

    struct thread * th = get_thread_data();
    // restore original thread stack (so we can use mem_free)
    armv8_set_sp(th->sp);
    // recover task data address
    u64 mem = (u64)th->tsk;
    th->tsk = 0;
    mem_free(pool4k, mem, 1);

    // yielding with a null task context returns
    // the thread to general circulation
    yield();

    printf("tsk: should never get here ...\n");
}


int
create_task(char * name, taskfn func, volatile void * arg)
{
    if (n_bits_set(RUNQ_MAX) != 1) {
        printf("RUNQ_MAX (%d) is not a power of 2\n", RUNQ_MAX);
        return 1;
    }

//#   define FAIL_IF_FULL
#   ifdef FAIL_IF_FULL
    if (mpmc_gate_count(runq) >= RUNQ_HIWAT) {
        printf("create_task: runq is full\n");
        return 1;
    }
#   else
    while (mpmc_gate_count(runq) >= RUNQ_HIWAT)
        yield_for(1);
#   endif

    struct task * tsk = (struct task *)mem_zalloc(pool4k, 1);
    if (tsk == 0) {
        printf("create_task: pool4k empty\n");
        return 1;
    }

    u64 stack = (u64)tsk + 0x1000;

    // target("omit-leaf-frame-pointer") issue ...
    stack -= STACK_EXTRA;

    int tid = atomic_fetch_inc(next_tid);

    tsk->name  = name;
    tsk->tid   = tid;
    tsk->func  = func;
    tsk->arg   = arg;
    tsk->lr    = (u64)&_start_task;
    tsk->sp    = stack;
    tsk->x0    = (u64)tsk;
    tsk->x19   = 0;
    tsk->x20   = 0;
    tsk->x21   = 0;
    //memset(&tsk->x19, 0, sizeof(tsk->x19) * 10);

    add_item(runq, tsk);
    printf("%s queued %lx\n", name, tsk);
    return 0;
}

// if the thread creating a task wants to join general circulation
// while waiting for completion of the created task, we need to
// be able to return to the current thread.
//
// yield_inclusive() queues the current thread context as an
// additional task that ONLY the current thread can resume ...
// its an unproven unthought-out epiphany ... but might prevent that
// last core from being idle.
void
yield_inclusive(void)
{
    delay(1000);

#if 0
    //volatile struct thread * th = get_thread_data();
    struct thread * th = get_thread_data();

    if (th->tsk) {
        // if this thread is a running task, save the restore point
        save_task_context(th->tsk);
        add_item(runq, th->tsk);
        th->tsk = 0;
    } else {
        // if there is no task context, create a fake task
        struct task * ltsk = (struct task *)th->scratch;        // zmalloc?
        printf("fake task %lx\n", ltsk);
        th->tsk = ltsk;
        memset(ltsk, 0, sizeof(struct task));
        ltsk->name  = "msleep";
        ltsk->tid = 0;
        //ltsk->tid   = atomic_fetch_inc(next_tid);
        ltsk->func  = 0;
        ltsk->arg   = 0;
        //ltsk->lr    = 0; //(u64)&_start_task;
        //asm volatile("mov %0, sp"  : "=r" (ltsk->sp) : );
        ltsk->x0    = 0; //(u64)tsk;
        //ltsk->x19   = 0;
        //ltsk->x20   = 0;
        //ltsk->x21   = 0;

        save_task_context(ltsk);
        add_item(runq, ltsk);
    }

    // get the next task to run (and wait for it if needed)

    remove_item_wait(runq, th->tsk, struct task *);

    if (th->tsk->tid == 0  &&  th->thno != cpu_id())
        printf("not mine %d cpu(%d)\n", th->tsk->tid, th->thno);

    // restore the selected task

    restore_task_context(th->tsk);
#endif

#if 0   // work-in-progress

    // put our marker in the runq
    if (th->tsk) {
        save_task_context(th->tsk);
        add_item(runq, th->tsk);
        th->tsk = 0;
    }

    remove_item_wait(runq, th->tsk, struct task *);

next:
    struct task * tsk;
    remove_item_wait(runq, tsk, struct task *);
    if (tsk->tid == 0  &&  th->thno != cpu_id()) {
        add_item(runq, tsk);                    // not ours, put it back
        goto next;                              // and try another
    }
    th->tsk = tsk;


    // restore the selected task

    restore_task_context(th->tsk);
#endif
}
