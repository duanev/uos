
/* -------- task support */

// FIXME gcc prologue for first task func writes above stack starting point!
// as a hack, move the stack starting position further down,
// and this might not be enough ... !!  (wish I knew a way to tell gcc to stop)
#define STACK_EXTRA     0x80

typedef void (*taskfn)(volatile void *);

int     create_task(char * name, taskfn func, volatile void * arg);
void    yield(void);
void    yield_for(int msecs);
void    yield_inclusive(void);
int     task_debug(void);

char  * get_task_name(void);
u64     get_task_id(void);

