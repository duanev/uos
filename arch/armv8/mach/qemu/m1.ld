
MEMORY
{
    code : org = 0x40080000 , len = 256
}

SECTIONS
{
    .text : { *(.text*) } > code
}
