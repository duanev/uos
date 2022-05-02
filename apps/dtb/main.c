
#include "uos.h"
#include "libdtb.h"

char * psci_method = 0;
char * console = 0;
int n_cpus = 0;
int cpuon = 0;

static void       save_cpuon(char * nn, u64 val) { cpuon = val; }
static void save_psci_method(char * nn, u64 val) { psci_method = (char *)val; }
static void       count_cpus(char * nn, u64 val) { n_cpus++; }
static void    pl011_devices(char * nn, u64 val) { printf("        pl011: %s\n", nn); }
static void     save_console(char * nn, u64 val) { if (console == 0) console = (char *)val; }


void
main(void)
{
    extern u32 * dtb;

    strcpy(get_tp(), "test...\n\0");
    con_puts(get_tp());
    sprintf(get_tp(), "one %s three\n", "two");
    con_puts(get_tp());

    printf("\xce\xbcos start dtb[%x] el%d tp0(%x)\n", *dtb, get_exec_level(), get_tp());

    if (is_dtb(dtb)) {
#       if 1
        //printf("console: %s\n", (char *)dtb_lookup(dtb, "chosen:stdout-path"));
        dtb_lookup(dtb, "psci:cpu_on", save_cpuon);
        printf("      psci-on: %x\n", cpuon);

        dtb_lookup(dtb, "psci:method", save_psci_method);
        printf("  psci method: %s\n", psci_method);

        dtb_lookup(dtb, "cpu@*:device_type", count_cpus);
        printf("         cpus: %d\n", n_cpus);

        dtb_lookup(dtb, "pl011@*:compatible", pl011_devices);

        dtb_lookup(dtb, "aliases:uart0", save_console);
        dtb_lookup(dtb, "chosen:stdout-path", save_console);    // if present, override uart0

        if (console)    printf("      console: %s\n", console);
        else            printf("no consoles found\n");

#       else
        dtb_dump(dtb);
#       endif
    }

    printf("\xce\xbcos end\n");
}
