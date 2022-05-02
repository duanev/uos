
MEMORY  /* 32K qemu-system-aarch64 -M virt */
{
    code : org = 0x40080000, len = 24K
    data : org = 0x40085000, len =  8K
    bss  : org = 0x40086000, len =  8K
    free : org = 0x40090000, len =  0K      /* needed for libmem and mem_init */
}

SECTIONS
{
    .text : { *(.text*) } > code

    data : {
        _data_start = .;

        *.o(.data)

        _data_end = .;
    } > data

    bss  : {
        _bss_start = .;

        *.o(.bss) *(COMMON)

        . = ALIGN(4K);
        _tls0 = .;
        _stack0_end = .;
        . += 1;
        . = ALIGN(4K);
        _stack0_start = .;

        _bss_end = .;
    } > bss

    free : {
        _free_mem = .;
    } > free
}
