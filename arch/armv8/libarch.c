
/*
 * stkdump()
 *
 * remember, compilers and their options can change the stack layout ...
 */

#include "uos.h"

#ifdef __GNUC__
#ifdef __ARM_ARCH_8A
#define ___supported
// stkdump() may only work for gcc -g -O1

// this sould more correctly be set to the total size of the stack,
// I'm reducing it here to make the frame hunt more strict - with of
// course, the caveat that large stack allocations will break stkdump()
#define MAX_LOCAL_ALLOC 0x200

void
stkdump(void)
{
    u64 * sp;
    u64 * pc;
    //u64 * fp;
    int i;

    asm volatile("mov %0, sp" : "=r" (sp) : );
    //asm volatile("mov %0, fp" : "=r" (fp) : );

    //printf("sp(%x) sp[0](%x) sp[1](%x) sp[2](%x) sp[3](%x) sp[4](%x) sp[5](%x)\n",
    //        sp, sp[0], sp[1], sp[2], sp[3], sp[4], sp[5]);

    // find previous sp - it should not be far, stkdump has no args
    for (i = 0; i < 4; i++) {
        if (sp[i] > (u64)sp  &&  sp[i] < ((u64)sp + MAX_LOCAL_ALLOC))
            break;
    }

    if (i >= 4) {
        printf("frame not found ... %x,%x,%x,%x\n", sp[i], sp[i+1], sp[i+2], sp[i+3]);
        return;
    }

    struct thread * th = get_thread_data();
    char * q = th->print_buf;
    q += sprintf(q, "\n*** stack: cpu(%d)\n", cpu_id());

    sp = (u64 *)sp[i];
    pc = (u64 *)sp[i+1];
    do {
        q += sprintf(q, "*** %d pc(%x) sp(%x) ret(%x)", cpu_id(), pc, sp, sp[0]);

        if (sp[0] < (u64)sp  ||  sp[0] > ((u64)sp + MAX_LOCAL_ALLOC)) {
            *q++ = '\n';
            break;
        }

        for (i = 2; (u64)(sp + i) < sp[0] ; i++)
            q += sprintf(q, " %x", sp[i]);
        pc = (u64 *)sp[1];
        sp = (u64 *)sp[0];
        *q++ = '\n';
    } while (1);

    q += sprintf(q, "press c to continue\n");
    *q = '\0';

    puts(th->print_buf);

    // Continue = 0; while (Continue == 0);
}

#endif
#endif

#ifndef ___supported
void
stkdump(void) {}
#endif

int
stkcheck(void)
{
    //struct thread * th = get_thread_data();
    u64 sp = get_sp();

    // the current stack is either:
    // a) a task stack,
    // b) an original thread stack
    // (both of which are currently 4k in size)
    // either way the stack grows down from a 4k boundary, so
    // *any* sp value within the bottom 1k means 3k stack usage ...
    // and that's a good a signal as any

    if ((sp & 0xc00) == 0) {
        printf("**** stack near overflow [0x%lx]\n", sp);
        stkdump();
        return 1;
    }

#if 0
    if (th->tsk) {
        if (sp < (u64)th->tsk + 1024) {
            printf("**** task(%s) thread stack near overflow [0x%lx]\n",
                            get_task_name(), sp);
            stkdump();
            return 1;
        }
    } else {
        if (sp < (u64)th + 1024) {
            printf("**** cpu(%d) thread stack near overflow [0x%lx]\n", cpu_id(), sp);
            stkdump();
            return 1;
        }
    }
#endif
    return 0;
}

