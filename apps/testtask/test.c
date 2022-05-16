
#include "uos.h"

extern volatile int Running;

/* ---- smp tests ----------------------------------------------- */

/*
 * test_smp_01 - only one cpu should execute test_smp_01_task at a time
 *               (ncpus take turns); this tests the task runq mpmc gate.
 */

static long volatile smp_test_array[MAX_CPUS] = {0};
static long volatile smp_loop_array[MAX_CPUS] = {0};
static long volatile smp_01_err = 0;
static int  ncpus;

void
smp_01_task(volatile void * arg)
{
    volatile int * np = (volatile int *)arg;
    int volatile smp_01_ok = 0;
    int id = cpu_id();
    printf("smp_01_task starting cpus(%d) loops(%d)\n", ncpus, *np);

    do {
        printf("+++ 1\n");
        //smp_test_array[id] = id + 1;
        atomic_store(smp_test_array[id], id + 1);

        for (int i = 0; i < ncpus; i++) {
            long val = atomic_fetch(smp_test_array[i]);
            if (i == id) {
                if (val != id + 1) {
                    printf("  test_smp_01 %d should be %d\n", val, id);
                    smp_01_err++;
                } else {
                    smp_01_ok++;
                }
            } else {
                if (val != 0) {
                    printf("  test_smp_01 %d should be 0\n", val);
                    smp_01_err++;
                } else {
                    smp_01_ok++;
                }
            }
        }

        if (smp_01_err > 1000000) {
            printf("smp_01_err %ld\n", smp_01_err);
            return;
        }

        if (smp_01_ok > 1000000) {
            smp_01_ok = 0;
            smp_loop_array[id]++;

            struct thread * th = get_thread_data();
            char * p = th->print_buf;
            p += sprintf(p, " test_smp_01 loops ");
            for (int i = 0; i < ncpus; i++)
                p += sprintf(p, " %4d", smp_loop_array[i]);
            sprintf(p, "  err(%d)\n", smp_01_err);
            puts(th->print_buf);
        }

        atomic_clear(smp_test_array[id]);

        yield();
        // note: new context here, previous cpu registers will be obsolete
        id = cpu_id();

        *np = *np - 1;
        printf("+++ 2\n");
    } while (*np);
}

/* ---- task tests ---------------------------------------------- */

// test that smp cores take turns running a single task.
// on qemu so far the sequence is well-ordered over all the
// circulating cpus (excludes cpu0 and cpu-last running the watchdog)
void
task_test0(volatile void * arg)
{
    volatile int * donep = (volatile int *)arg;
    u64 now = etime();
    u64 end = now + 10000;

    printf("test0 until(%ld)\n", end);

    do {
        yield();
        now = etime();
        printf("test0 now(%ld)\n", now);
    } while (end > now);

    *donep = 1;
}


// test passing parent task data to/from children
struct test1_ptd {
    char * m1;
    char * m2;
    char * m3;
    volatile int    running;
};

// test data passing to children
void
task_test1_sleep(volatile void * arg)
{
    volatile struct test1_ptd * ptd = (volatile struct test1_ptd *)arg;

    printf("test1.sleep message 1 [%s]\n", ptd->m1);
    ptd->m1 = "un";
    delay(1000);
    printf("test1.sleep message 2 [%s]\n", ptd->m2);
    ptd->m2 = "deux";
    printf("test1.sleep 4 seconds ...\n");
    yield_for(4000);
    printf("test1.sleep message 3 [%s]\n", ptd->m3);
    ptd->m3 = "trois";
    ptd->running = 0;
}

// queue 6 more printf tasks every second.
// shorten the task runq to test hi-water create_task rejection.
// test the return code pointer
void
task_test2(volatile void * arg)
{
    volatile int * rcp = (volatile int *)arg;
    int err = 0;
    int n = 1000;           // over time, create 6000 tasks
    while (Running  &&  n-- > 0) {
        err += create_task("one  ", (taskfn)printf, "one   says hi\n");
        err += create_task("two  ", (taskfn)printf, "two   says hi\n");
        err += create_task("three", (taskfn)printf, "three says hi\n");
        err += create_task("four ", (taskfn)printf, "four  says hi\n");
        err += create_task("five ", (taskfn)printf, "five  says hi\n");
        err += create_task("six  ", (taskfn)printf, "six   says hi\n");
    }
    printf("test2 complete - task_errs(%lld)\n", err);
    *rcp = 1;
}

// recursively queue the task named "thr" until resources are exhausted:
// either runq space (depending on how big the queue and if the create_task()
// thread spins for free runq space), or 4k buffers for the task struct and
// stack, or the task_names array here reaches its limit (1x4k => 512 names,
// adjust NAME_SPACE to increase).
#define NAME_SPACE  1       // pool4k - 4k units
#define NAME_SIZE   8
#define NAME_MAX    (NAME_SPACE * 4096 / NAME_SIZE)
static char * task_names;
static volatile int tname_idx = 4;

void
task_test3(volatile void * arg)
{
    char * a0 = (char *)arg;    // ok to discard volatile - nothing gets returned
    if (Running  &&  a0[0] == 't'  &&  a0[1] == 'h'  &&  a0[2] == 'r') {
        //int idx = atomic_fetch_inc(tname_idx);                  // stops
        int idx = atomic_fetch_inc(tname_idx) & (NAME_MAX - 1); // runs forever
        if (idx < NAME_MAX) {
            char * name = task_names + (idx * NAME_SIZE);
            sprintf(name, "%06d", idx);         // must be < NAME_SIZE chars
            if (create_task(name, task_test3, arg))
                printf("failed to queue %s\n", name);
        }
    }

    int n = 27;
    while (n-- > 0) {
        yield();
        printf("%s %s  n(%d)\n", get_task_name(), (char *)arg, n);
    }
}

/* ---- test main ----------------------------------------------- */

int
test(int suite)
{
    int err = 0;

    printf(" ---- test 0x%x ------------------------------------\n", suite);

    if (suite == 0) {
#       ifdef calibrate_time
        // eyeball the start wall clock time and record the 'start' count,
        // wait a few hours, or days,
        // eyeball the end wall clock time and the latest 'clock' count,
        // then do the math.
        u64 clock = etime() + 1000000;      // about 1 second in the future
        printf("start %016lx\n", clock);
        while (1) {
            if (etime() > clock) {
                clock += 1000000 ;
                printf("clock %016lx\n", clock);
            }
#           ifdef cn96
            extern void cn96_watchdog_reset(void);
            cn96_watchdog_reset();
#           endif
        }
#       endif

    } else if ((suite & 0xf0) == 0x20) {
        ncpus = suite & 0xf;
        int n = 10;
        create_task("test_smp_01", smp_01_task, (volatile void *)&n);
        while (n) delay(1);
        printf("test_smp_01 complete\n");
        return 0;

    } else if ((suite & 0xf0) == 0x30) {

        create_task("printf", (taskfn)printf, "printf test says hi!\n");

        switch (suite & 0xf) {
        case 0x0:
            volatile int done = 0;
            // let all cpus run a single task for 1 second
            create_task("test0", task_test0, &done);
            while (!done)
                yield_inclusive();
            break;

        case 0x1:
            volatile struct test1_ptd t1ptd = {"uno", "dos", "tres", 1};
            // test sharing a struct between parent and child
            create_task("test1.sleep1", task_test1_sleep, &t1ptd);
            while (t1ptd.running) delay(1);
            printf("test1 complete: %s %s %s\n", t1ptd.m1, t1ptd.m2, t1ptd.m3);
            break;

        case 0x2:
            // task tests should run the same regardless
            // of the number of cpu threads
            {   volatile int rc = 0;
                create_task("test2", task_test2, &rc);
                while (rc == 0) delay(1);
                // thread0 can exit here even when dozens of tasks
                // are queued and will still run - so scroll back
                // to look for 'complete' in the console output
                printf("test2 complete\n");
            }
            break;

        case 0x3:
            // get a buffer for task names
            task_names = (char *)mem_alloc(pool4k, NAME_SPACE);
            if (task_names == 0)
                printf("**** alloc_failed for task_names\n");
            create_task("test3 task002", task_test3, "two");
            create_task("test3 task003", task_test3, "three");
            printf("parent cpu yielding ...\n");
            // using yield() here never returns, however for now
            // I want to make sure cpu0 can join the other cpus.  
            yield();
#           ifdef eventually
            while (Running)
                yield_inclusive();
#           endif
            break;
/*
*/
        }
        return 0;
    }

    printf("unknown test 0x%x\n", suite);
    return 1;
}
