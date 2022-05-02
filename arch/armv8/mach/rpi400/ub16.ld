MEMORY
{
    code : org = 0x01000000, len = 8K
    data : org = 0x01002000, len = 2K
    bss  : org = 0x01002800, len = 6K
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
}
