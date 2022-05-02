
#include "uos.h"

void
exc_handler(void)
{
    con_puts("excption test: pass\n");
}

void puts(const char * buf)
{
    con_puts(buf);
}

void
main(void)
{
    puts("\xce\xbcos hello world\n");
    cause_exc();
}
