
// describe the thread local storage area,
// this is private per-core RAM

struct thread {
    u64             sp;         // boot.s expects this to be first
                                // also used by libtask
    u64             thno;
    struct task *   tsk;
    void         (* func)(struct thread *);
    volatile void * arg0;
    u64             _res1;
    u64             _res2;      // align scratch/print_buf to a
    u64             _res3;      // cache line (easier to debug)

    char            scratch[64];
    char            print_buf[0];
};

int    smp_init(int max_cpus);
int    smp_start_thread(void (* func)(struct thread *), void * arg0);

