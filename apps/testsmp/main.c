
#include "uos.h"

int Running = 1;

void
watchdog(struct thread * th)
{
    printf("watchdog thread %d: el%d arg(%lx)\n", th->thno, get_exec_level(), th->arg0);

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

// with no task management, yield() is just pause()
void
yield(void)
{
    while (1) pause();
}

void
work_thread(struct thread *th)
{
    do {
        delay(50000);
        printf("thread %d\n", th->thno);
    } while (Running);
}

void
main(int ac, char * av[])
{
    printf("testsmp v0.90 2019/09/21 el%d\n", get_exec_level());

    mem_init();

    smp_start_thread(work_thread, 0);
    smp_start_thread(work_thread, 0);
    // assign the fourth cpu to run the watchdog thread
    smp_start_thread(watchdog, 0);
    printf("[5mpress 'q' five times to quit[0m\n");

    // and cpu0 can go do the work_thread too
    struct thread * th = get_thread_data();
    work_thread(th);
}
