// rpi400 uos boot and crt0

// ---- start ------------------------------

.global _code_start
.global exc_handler

.section .text
_code_start:
start:
    ldr     x0, =exc_table1         // setup the el1 exception vectors (exc.s)
//  msr     vbar_el1, x0            // if using ?
    msr     vbar_el2, x0            // if using start4.elf

    /**********************************************
    * Set up memory attributes (worked on a53, what about a72?)
    * This equates to:
    * 0 = b01000100 = Normal, Inner/Outer Non-Cacheable
    * 1 = b11111111 = Normal, Inner/Outer WB/WA/RA
    * 2 = b00000000 = Device-nGnRnE
    * 3 = b00000100 = Device-nGnRE
    * 4 = b10111011 = Normal, Inner/Outer WT/WA/RA
    **********************************************/
    ldr     x0, =0x000000BB0400FF44
    msr     mair_el1, x0
    isb

//  mrs     x0, sctlr_el1
//  orr     x0, x0, #0x1000         // enable I cache: %60 faster execution speed?
//  orr     x0, x0, #0x0004         // enable D cache: (no effect?)
//  msr     sctlr_el1, x0
//  dsb     sy
//  isb

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

    ldr     x0, =msg_boot
    bl      con_puts

    // ---- call main
//  mov     x30, xzr
//  mov     x29, xzr

    bl      main

    ldr     x0, =msg_done
    bl      con_puts

//  ldr     x0, =0x84000009         // system reset
//  hvc     0                       // secure monitor / hypervisor call fn 0
    b .                             // hang

// ---- pl011 uart -------------------------

UART_DR  = 0x00
UART_FR  = 0x18

FR_TXFF = (1 << 5)
FR_RXFE = (1 << 4)
FR_BUSY = (1 << 3)

ASCII_NL = 10
ASCII_CR = 13

    .macro wait_for_tx_ready
    1:
        ldr     w3, [x1, UART_FR]
        mov     w4, #(FR_TXFF | FR_BUSY)
        and     w3, w3, w4
        cbnz    w3, 1b
    .endm

    .global con_puts
    .global con_peek
    .global con_getc

    // x0 points to string
    .global con_puts
con_puts:
    ldr     x1, uart_addr
loop:
    wait_for_tx_ready
    ldrb    w2, [x0]
    cmp     x2, #0
    b.eq    done
    cmp     w2, ASCII_NL
    b.ne    2f
    mov     w3, ASCII_CR
    str     x3, [x1]
    wait_for_tx_ready
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
    .dword 0x0000fe201000           // rpi400 serial0

.section .data

    .global dtb
dtb:
    .dword 0x2eff4100,0             // start4 dtb

msg_boot:
    .byte   0xce, 0xbc
    .ascii  "os boot\n"
    .byte   0

msg_done:
    .byte   0xce, 0xbc
    .ascii "os done\n"
    .byte  0

