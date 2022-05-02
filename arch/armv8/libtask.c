
#include "uos.h"


struct task {
    u64             tid;
    u64             flags;
    char          * name;
    u64             sleep;

    taskfn          func;
    volatile void * arg;                        // volatile in case data is returned

    u64             sp;
    u64             x0;

    // all the callee-saved regs
    // https://modexp.wordpress.com/2018/10/30/arm64-assembly
    u64             x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, fp, pc;

    // ensure the registers above remain in order
#   define  X19_OFF __builtin_offsetof(struct task, x19)

    // keep struct size a multiple of 64 (currently 192)
    // and keep sp....pc all in the same place!

    u64             _res1, _res2, _res3, _res4;
};

static u64 next_tid = 1;


#define RUNQ_MAX    (MAX_CPUS * 16)
#define RUNQ_HIWAT  (RUNQ_MAX - MAX_CPUS)       // guarantee room for running threads

MPMC_GATE_INIT(runq, RUNQ_MAX);

static inline void
armv8_set_sp(u64 sp)
{
    asm volatile("mov sp, %0" : : "r" (sp) : );
}

void inline
save_task_context(struct task * tsk)
{
    asm ("stp x19, x20, [x0, %c[x19o]]\n"
         "stp x21, x22, [x0, %c[x19o] + 0x10]\n"
         "stp x23, x24, [x0, %c[x19o] + 0x20]\n"
         "stp x25, x26, [x0, %c[x19o] + 0x30]\n"
         "stp x27, x28, [x0, %c[x19o] + 0x40]\n"
         "stp fp,  lr,  [x0, %c[x19o] + 0x50]\n" : : [x19o] "i" (X19_OFF) );

    asm volatile("mov %0, sp"  : "=r" (tsk->sp) : );
}

void inline
restore_task_context(struct task * tsk)
{
    asm ("ldp x19, x20, [x0, %c[x19o]]\n"
         "ldp x21, x22, [x0, %c[x19o] + 0x10]\n"
         "ldp x23, x24, [x0, %c[x19o] + 0x20]\n"
         "ldp x25, x26, [x0, %c[x19o] + 0x30]\n"
         "ldp x27, x28, [x0, %c[x19o] + 0x40]\n"
         "ldp fp,  lr,  [x0, %c[x19o] + 0x50]\n" : : [x19o] "i" (X19_OFF) );

    asm volatile("mov sp,  %0" : : "r" (tsk->sp) : );
    // gcc needs x0 loaded last (go look at the disassembly)
    asm volatile("mov x0,  %0" : : "r" (tsk->x0) : );
}

// noinline forces a consistent stack frame
void __attribute__ ((noinline))
yield(void)
{
    struct thread * volatile th = get_thread_data();

    if (th->tsk) {
        // if this thread is a running task, save the restore point
        save_task_context(th->tsk);
        add_item(runq, th->tsk);
        th->tsk = 0;
    }

    // get the next task to run (and wait if needed)

    remove_item_wwait(runq, th->tsk, struct task *);

    // restore the selected task

    restore_task_context(th->tsk);
}

void __attribute__ ((noinline))
usleep(int usecs)
{
    struct thread * th = get_thread_data();
    struct task * tsk = th->tsk;
    tsk->sleep = usecs + etime();

    while (tsk->sleep > etime()) {
        yield();
        // this printf works well with TASK_TEST0 showing which cpus pick up this task
        //printf("  %10x   %10d > %10d\n", tsk, tsk->sleep, etime());
    }
}

// when a thread runs a task for the first time,
// start_task() creates the initial stack frame

void
_start_task(struct task * tsk)
{
    printf("task %ld [%s] starting   [arg=0x%x,tsk=0x%x]\n", tsk->tid, tsk->name, tsk->arg, tsk);

    tsk->func(tsk->arg);

    printf("task %ld [%s] ending\n", tsk->tid, tsk->name);

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
#endif

#if 0
void
task_debug(void)
{
    debug_mpmc_gate(runq, task_dbg);
}
#endif


int
create_task(char * name, taskfn func, volatile void * arg)
{
    if (n_bits_set(RUNQ_MAX) != 1) {
        printf("RUNQ_MAX (%d) is not a power of 2\n", RUNQ_MAX);
        return 1;
    }

    //debug_mpmc_gate(runq, task_dbg);

//#   define FAIL_IF_FULL
#   ifdef FAIL_IF_FULL
    if (mpmc_gate_count(runq) >= RUNQ_HIWAT) {
        printf("create_task: runq is full\n");
        return 1;
    }
#   else
    while (mpmc_gate_count(runq) >= RUNQ_HIWAT)
        usleep(100);
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
    tsk->pc    = (u64)&_start_task;
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
