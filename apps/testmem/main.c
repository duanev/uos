
#include "uos.h"

int test(int);

void
main(void)
{
    extern u32 * dtb;

    printf("\xce\xbcos start dtb[%x] el%d\n", *dtb, get_exec_level());

    mem_init();

    //test(0);                        // calibrate time (by hand)

    test(0x11);
    test(0x12);

    printf("\xce\xbcos end\n");
}
