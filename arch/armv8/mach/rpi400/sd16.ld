MEMORY
{
    code : org = 0x00080000, len = 16K
    data : org = 0x00084000, len =  4K
    bss  : org = 0x00085000, len = 12K
    free : org = 0x00090000, len =  0K      /* needed for libmem and mem_init */
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
