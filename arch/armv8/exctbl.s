
    .macro vector1 vecno label
    .align 7
    \label:
    mov x0,  \vecno
    mrs x1,  esr_el1        // exception syndrome reg
    mrs x2,  elr_el1        // exception link reg
    mrs x3,  far_el1        // fault address reg
    bl  exc_handler
    b   .
    .endm

    .align 9
    .global exc_table1
exc_table1:

    vector1  0 sp0_sync1
    vector1  1 sp0_irq1
    vector1  2 sp0_fiq1
    vector1  3 sp0_serror1
    vector1  4 spx_sync1
    vector1  5 spx_irq1
    vector1  6 spx_fiq1
    vector1  7 spx_serror1
    vector1  8 el64_sync1
    vector1  9 el64_irq1
    vector1 10 el64_fiq1
    vector1 11 el64_serror1
    vector1 12 el32_sync1
    vector1 13 el32_irq1
    vector1 14 el32_fiq1
    vector1 15 el32_serror1

