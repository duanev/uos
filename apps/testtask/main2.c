
/*
 * a stripped down version of testtask/main.c
 * that only validates yield_inclusive().
 * substantially easier to debug ...

$ cat apps/testtask/mkvars
SRCS += libarch libc libmem libbitmap mem_init libsmp libtask main2
DFLAGS += -DMAX_CPUS=2
QEMU_RUN_CMD += -smp 2
MEM_SIZE = 32

 */

#include "uos.h"

volatile int Initializing = 1;
volatile int Running = 1;

void
main(int ac, char * av[])
{
    printf("testtask el%d\n", get_exec_level());

    mem_init();

    int ncpus = smp_init(1);

    u64 i = 0;
    while (1) {
        printf("main i(%ld)\n", i);
        yield_inclusive();
    }

    printf("testtask complete\n");
}
