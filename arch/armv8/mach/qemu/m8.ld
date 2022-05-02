
MEMORY  /* 8K qemu-system-aarch64 -M virt */
{
    code : org = 0x40080000, len = 6K
    data : org = 0x40081800, len = 2K
    bss  : org = 0x40082000, len = 2K
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

        . = ALIGN(1K);
        _tls0 = .;
        _stack0_end = .;
        . += 1;
        . = ALIGN(1K);
        _stack0_start = .;

        _bss_end = .;
    } > bss
}
