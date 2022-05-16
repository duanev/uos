
/*
$ cat apps/testtask/mkvars
SRCS += libarch libc libmem libbitmap mem_init libsmp libtask test main
DFLAGS += -DMAX_CPUS=8
QEMU_RUN_CMD += -smp 8
MEM_SIZE = 32
*/

#ifdef QEMU_VIRT
#define RUN_OS_TEST 3
#endif

#include "uos.h"
#include "test.h"


volatile int Initializing = 1;
volatile int Running = 1;

void
watchdog(struct thread * th)
{
    printf("watchdog thread %d: el%d arg(%lx)\n", th->thno, get_exec_level(), th->arg0);

    Initializing = 0;

    int n = 0;
    while (n < 5) {
        if (con_peek()) {
            int c = con_getc();
            printf("getc: 0x%x\n", c);
            n = (c == 'q') ? n + 1 : 0;
        }
    }

    Running = 0;
}

void
main(int ac, char * av[])
{
    printf("testtask el%d\n", get_exec_level());

    mem_init();

    int ncpus = smp_init(6);        // ask for a few more in addition to cpu0

    // assign one more cpu/core to run the watchdog thread
    if (smp_start_thread(watchdog, 0) != 0) {
        printf("insufficient cpus (%d) to start watchdog, aborting.\n", ncpus);
        return;
    }
    printf("[5mpress 'q' five times to quit[0m\n");

    while (Initializing) delay(1);

    do {
#       if RUN_OS_TEST==2
        TEST(0x20 + ncpus);
#       endif

#       if RUN_OS_TEST==3
        TEST(0x30);
        TEST(0x31);
        delay(100000);
        TEST(0x32);
        delay(100000);
        TEST(0x33);                 // runs until !Running
/*
*/
#       endif
    } while(0);                     // allow break to work

    printf("testtask complete\n");
}
