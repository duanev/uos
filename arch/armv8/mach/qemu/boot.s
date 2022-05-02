// qemu uos boot and crt0

// ---- start ------------------------------

.global _code_start
.global exc_handler

.section .text
_code_start:
    adr     x5, dtb                 // save the dtb pointer
    stp     x0, x1, [x5]

    ldr     x0, =exc_table1         // setup the el1 exception vectors (exc.s)
    msr     vbar_el1, x0
//  msr     vbar_el2, x0            // for virtualization=on

    // ---- clear bss

    mov     x0, #0
    ldr     x1, =_bss_start
    ldr     x2, =_bss_end
1:
    str     x0, [x1], #8
    cmp     x1, x2
    blt     1b

    // ---- init the cpu0 stack and thread0 local storage pointer

    ldr     x0, =_stack0_start
    mov     sp, x0
    ldr     x0, =_tls0
    msr     tpidr_el0, x0

    // ---- call main

    ldr     x0, =msg_boot
    bl      con_puts

    bl      main

    ldr     x0, =msg_done
    bl      con_puts

    // ---- if main returns

reboot:
    // actually, better to hang on qemu than restart
#   if 0
    ldr     x0, =1000000000         // qemu: about 3 seconds
1:
    sub     x0, x0, #1
    cbnz    x0, 1b                  // delay so con_puts can be seen
    ldr     x0, =msg_reboot
    bl      con_puts

    ldr     x0, =0x84000009         // system reset
    hvc     0                       // hypervisor call
#   endif

    b       .                       // hang

#   if MAX_CPUS > 1
    .global smp_entry
smp_entry:
    ldr     x3, =exc_table1         // setup the el1 exception vectors (exc.s)
    msr     vbar_el1, x3

    msr     tpidr_el0, x0
    ldr     x3, [x0]
    mov     sp, x3
    .extern smp_newcpu              // provided by smp.c
    bl      smp_newcpu
    b       .                       // hang
#   endif

// ---- pl011 uart -------------------------

UART_DR  = 0x00
UART_FR  = 0x18

FR_TXFF = (1 << 5)
FR_RXFE = (1 << 4)
FR_BUSY = (1 << 3)

ASCII_NL = 10
ASCII_CR = 13

    .global con_puts
    .global con_peek
    .global con_getc

con_puts:
    ldr     x1, uart_addr
loop:
    ldrb    w2, [x0]
    cmp     x2, #0
    b.eq    done
    cmp     w2, ASCII_NL
    b.ne    2f
    mov     w3, ASCII_CR
    str     x3, [x1]
2:
    str     x2, [x1]
    add     x0, x0, #1
    b       loop
done:
    ret

con_peek:
    mov     x0, #0
    ldr     x1, uart_addr
    ldr     w2, [x1, UART_FR]
    and     w2, w2, #FR_RXFE
    cbnz    w2, 1f
    add     x0, x1, #1
1:
    ret

con_getc:
    ldr     x1, uart_addr
1:
    ldr     w2, [x1, UART_FR]
    and     w2, w2, #FR_RXFE
    cbnz    w2, 1b
    ldr     w0, [x1, UART_DR]
    ret


    .align 4
uart_addr:
    .dword 0x000009000000       // qemu-system-aarch64 -M virt


.section .data

    .global dtb
dtb:
    .dword 0,0

msg_boot:
    .byte   0xce, 0xbc
    .ascii  "os boot\n"
    .byte   0

msg_done:
    .byte   0xce, 0xbc
    .ascii "os done\n"
    .byte  0

msg_reboot:
    .ascii "rebooting ...\n"
    .byte  0

