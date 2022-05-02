
#include "uos.h"


// armv8 exception names
static char * exc_vec_labels[16] = {
    "sp0_sync",  "sp0_irq",  "sp0_fiq",  "sp0_serror",
    "spx_sync",  "spx_irq",  "spx_fiq",  "spx_serror",
    "el64_sync", "el64_irq", "el64_fiq", "el64_serror",
    "el32_sync", "el32_irq", "el32_fiq", "el32_serror"
};

void
exc_handler(u64 vecno, u64 esr, u64 elr, u64 far)
{
#   ifdef DEBUG_PRINTF
    // using con_puts directly - in case printf caused the exception
    con_puts("*** excption: ");
    con_puts(exc_vec_labels[vecno]);
    con_puts("\n");
    printf("*** cpu(%d) %s esr(%x) elr(%x) far(%x)\n",
            _cpu_id(), exc_vec_labels[vecno], esr, elr, far);
#   else
    printf("*** exception cpu(%d) %s esr(%x) elr(%x) far(%x)\n",
            _cpu_id(), exc_vec_labels[vecno], esr, elr, far);
#   endif

    //stkdump();
}

