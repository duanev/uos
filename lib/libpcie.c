/*
 *  PCIe support    (a work in progress)
 *
 *  ... ldev arm/c/pcie-dbg1
 *
 * notes:
 *  type 0 config header   PCI-Express-Base-Spec-v3.0.pdf  pg 595
 *                         https://en.wikipedia.org/wiki/PCI_configuration_space
 *
 *  https://stackoverflow.com/questions/19006632/how-is-a-pci-pcie-bar-size-determined
 *  https://wiki.osdev.org/PCI
 *  https://wiki.osdev.org/PCI_Express
 *  https://wiki.osdev.org/PCI#Enumerating_PCI_Buses
 *  https://wiki.qemu.org/images/f/f6/PCIvsPCIe.pdf
 */

#define DEBUG
//#define DEBUG_CAP

#ifdef QEMU_VIRT
#define PCIE_ECAM_BASE      0x4010000000ull
#define PCIE_MEM_START      0x0010000000ull     // this likely requires -m 256 or less
#define PCIE_PMEM_START     0x0020000000ull
#define PCIE_BAR_SIZE       0x0000100000ull     // 1MB is the minimum non-prefetchable size
#else
#error "pcie ecam location unknown"
#endif

#define MIN_BAR_SIZE        0x10000             // FIXME this is a guess

#include "uos.h"
#include "pcie.h"


static char Pfx[64] = {0};                      // FIXME overflow risk for deep bus hierarchies

static int
push_pfx(char * str)
{
    int idx = strlen(Pfx);
    strcpy(Pfx + idx, str);
    return idx;
}

static void
pop_pfx(int idx)
{
    Pfx[idx] = '\0';
}

struct nvme_device {
    u64 b0;
    u64 b4;
} Nvme_devices[8];
int Nnvme_devices = 0;

static u64 Next_bar = PCIE_MEM_START;
static int Next_bus = 1;                                // 0 is the cpu root port

struct start_and_limit {
    u64 pmem_start;
    u64 pmem_limit;
    u64 mem_start;
    u64 mem_limit;
};

//                  0x....f0000000      a << 28         // multiuple adjacent ecam spaces?
// bus              0x.....ff00000      a << 20
// device           0x.......f8000      a << 15
// function         0x........7000      a << 12

#define PCIE_CONF(s,o,b,d) (struct pcie_confspace *)((u64)(s) | ((u64)(o) << 28) | \
                                                    ((u64)(b) << 20) | ((u64)(d) << 15))

#define PCIE_CONF_FN(s,o,b,d,f) (struct pcie_confspace *)((u64)(s) | ((u64)(o) << 28) | \
                                 ((u64)(b) << 20) | ((u64)(d) << 15) | ((u64)(f) << 12))

void
pcie_config_device(volatile struct pcie_confspace * conf, struct start_and_limit * snl)
{
    int n_bars = ((conf->headerType & 0x7f) == 1) ? 2 : 6;    // bridges have 2 bars

    // encourage bars to expose their size
    for (int j = 0; j < n_bars; j++)
        conf->bar[j] = 0xffffffff;

    // show which bars want to be configured (non-zero values)
#   if 0
    printf("+++ conf->bar 5,4,3,2,1,0 %x %x %x %x %x %x\n",
                    conf->ep.bar5, conf->ep.bar4,
                    conf->ep.bar3, conf->ep.bar2,
                    conf->bar[1], conf->bar[0]);
#   endif

    for (int j = 0; j < n_bars; j++) {
        u32 bar = conf->bar[j];
        if (bar) {
            u64 siz = 0xffffffff00000000ull | (bar & PCI_CONF_BAR_MASK);
            int prfch = ((bar & PCI_CONF_BAR_PREFETCH) != 0);
            int b64   = ((bar & PCI_CONF_BAR_64BIT) != 0);
            if (b64)
                siz |= ((u64)conf->bar[j+1] << 32);

            siz = ~siz + 1;
#           ifdef DEBUG
            printf("%s %lx bar%d %s subven(%x) subid(%x) size(%lx)%s [%lx]\n", Pfx,
                    conf->bar + j, j, b64 ? "64" : "32",
                    conf->ep.subsysVendorId, conf->ep.subsysId, siz,
                    prfch ? " prefetch" : "", Next_bar);
#           endif

            if (prfch) {
                if (b64) {
                    //*(u64 *)(conf->bar + j) = Next_bar;   does not work!
                    conf->bar[j]   = Next_bar & 0xffffffff;
                    conf->bar[j+1] = Next_bar >> 32;
                } else
                    conf->bar[j] = Next_bar;        // can index out of bounds (its ok)

                if (snl->pmem_start == 0)
                    snl->pmem_start = Next_bar;
                snl->pmem_limit = Next_bar + siz;

                printf("%s +++ bar%d pmem base(%lx) limit(%lx)\n", Pfx, j, snl->mem_start, snl->mem_limit);

            } else {
                if (siz > PCIE_BAR_SIZE)
                    printf("%s bar%d **** requested size %x (but we only give %x) ****\n", Pfx,
                            conf->bar + j, siz, PCIE_BAR_SIZE);

                if (b64) {
                    //*(u64 *)(conf->bar + j) = Next_bar;   does not work!
                    conf->bar[j]   = Next_bar & 0xffffffff;
                    conf->bar[j+1] = Next_bar >> 32;
                } else
                    conf->bar[j] = Next_bar;        // can index out of bounds (its ok)
                //dc_flush((u64)conf);
                printf("%s +++ conf->bar[%d] %x %x\n", Pfx, j, conf->bar[j], Next_bar);

                if (snl->mem_start == 0)
                    snl->mem_start = Next_bar;
                snl->mem_limit = Next_bar + siz;
                printf("%s +++ bar%d mem base(%lx) limit(%lx)\n", Pfx, j, snl->mem_start, snl->mem_limit);
            }

            // notice NVMe devices
            if (conf->classCode == 1  &&  conf->subclass == 8) {
                int b64 = ((conf->bar[0] & PCI_CONF_BAR_64BIT) != 0);
                if (!b64)
                    printf("**** warning: NVME drive with 32bit bar - untested (%x)\n", conf);
                Nvme_devices[Nnvme_devices].b0 = conf->bar[0]  & PCI_CONF_BAR_MASK;
                Nvme_devices[Nnvme_devices].b4 = conf->ep.bar4 & PCI_CONF_BAR_MASK;
                Nnvme_devices++;
            }

            Next_bar += max(siz, MIN_BAR_SIZE);
            //Next_bar += MIN_BAR_SIZE;

            if (b64) j++;
        }
    }

    /* -------- list device capabilities -------- */

#   ifdef DEBUG_CAP
    if (conf->capPtr) {
        struct pcie_capability * cap = (void *)conf + conf->capPtr;
        for (; cap->next; cap = (void *)conf + cap->next) {
            static char capstr[128];
            char * p = capstr;
            if (cap->capId < LEN(cap_names))
                p += sprintf(p, "%s %lx cap %6s", Pfx, cap, cap_names[cap->capId]);
            else
                p += sprintf(p, "%s %lx cap id(%x)", Pfx, cap, cap->capId);
            //printf("%s  root port - bus(%x) capid(%x) id(%x)", Pfx, BUS(conf), cap, cap->capId);
            if (cap->capReg)  p += sprintf(p, " reg(%x)", cap->capReg);
            if (cap->devCap)  p += sprintf(p, " dev(%x,%x)", cap->devCap, cap->devStatus);
            if (cap->linkCap) p += sprintf(p, " link(%x,%x)", cap->linkCap, cap->linkStatus);
            if (cap->slotCap) p += sprintf(p, " slot(%x,%x)", cap->slotCap, cap->slotStatus);
            sprintf(p, "\n");
            printf(capstr);
        }
    }
#   endif
}

void
pcie_scan_device(volatile struct pcie_confspace * conf_bus, struct start_and_limit * snl)
{
    for (int fn = 0; fn < 8; fn++) {
        volatile struct pcie_confspace * conf_dev = PCIE_CONF_FN(conf_bus, 0, 0, 0, fn);
        if (conf_dev->vendorId == 0xffff) continue;

        printf("%s.%d %lx [%04x:%04x] (%d,%d,%d)\n", Pfx, fn, conf_dev, conf_dev->vendorId, conf_dev->deviceId,
                          conf_dev->classCode, conf_dev->subclass, conf_dev->progIf);

        char fpfx[8];
        sprintf(fpfx, ".%d", fn);
        int pp = push_pfx(fpfx);
        {
            pcie_config_device(conf_dev, snl);
        }
        pop_pfx(pp);
    }
}

void
pcie_scan_bus(volatile struct pcie_confspace * base,
              volatile struct pcie_confspace * conf_bus, struct start_and_limit * upstrm)
{
    int bus = ((u64)conf_bus >> 20) & 0xff;

    printf("%s ---- scanbus %lx\n", Pfx, conf_bus);

    for (int dev = 0; dev < 32; dev++) {              // dev0 is the bus
        volatile struct pcie_confspace * conf_dev = PCIE_CONF(base, 0, bus, dev);
        //printf("%s -------- bus dev %lx (%d,%d,%d)\n", Pfx, conf_dev, conf_dev->classCode, conf_dev->subclass, conf_dev->progIf);
        if (conf_dev->vendorId == 0xffff) continue;


        char dpfx[8];
        sprintf(dpfx, ".%02x", dev);
        int pp = push_pfx(dpfx);
        {
            //printf("%s %lx [%04x:%04x] (%d,%d,%d)\n", Pfx, conf_dev, conf_dev->vendorId, conf_dev->deviceId,
            //              conf_dev->classCode, conf_dev->subclass, conf_dev->progIf);

            // do we care about the multifunction flag?  can we rely on 0xffff in the vendorId?
            if (conf_dev->headerType & 0x80)
                printf("%s multifunc ...\n", Pfx);


            if ((conf_dev->headerType & 0x7f) != 1) {
                pcie_scan_device(conf_dev, upstrm);
                conf_dev->command = PCI_CMD_DEV;

            } else {
                int bus = Next_bus++;

                // its nice that sowfware and hardware bus numbers coincide, but call me skeptical!
                if (((u64)conf_dev >> 20) & 0xff != bus)
                    printf("+++ hw(%d) sw(%d) bus numbers are different\n", ((u64)conf_dev >> 20) & 0xff, bus);

                printf("%s dev0 is a bridge, assigning %02x to \n", Pfx, bus);
                conf_dev->command = 0;
                conf_dev->rp.priBus = bus;
                conf_dev->rp.secBus = Next_bus;
                conf_dev->rp.subBus = 255;              // enable access to all subordinates?
                dc_flush((u64)conf_dev);
                //pcie_config_device(conf_dev, snl);

                //delay(1000);

                // if there are downstream buses, update subBus
                if (Next_bus > conf_dev->rp.priBus + 1  &&  conf_dev->rp.subBus == 0) {
                    //int bus = ((u64)conf_dev >> 20) & 0xff;
                    printf("%s config bus %02x limit sub to %d?\n", Pfx, conf_dev->rp.priBus, Next_bus - 1);
                    conf_dev->rp.subBus = Next_bus - 1;
                }

                /* -------- propagate bus mmio limits to the upstream busses -------- */
                // ...

                conf_dev->command = PCI_CMD_BUS;
            }
        }
        pop_pfx(pp);
    }
}


// ---- sample device
#include "nvme.h"

void
show_nvme(struct nvme_bar * bar0)
{
    // set mps(4k), queue entry sizes, and enable controller
    bar0->cc = NVME_CC_ENABLE | NVME_CC_MPS
                              | (LOG(sizeof(struct nvme_sqe)) << 16)
                              | (LOG(sizeof(struct nvme_cqe)) << 20);

    printf("nvme drive: cap(%lx) vs(%x) cc(%x) csts(%x) asq(%x) acq(%x)\n",
                        bar0->cap, bar0->vs, bar0->cc, bar0->csts, bar0->asq, bar0->acq);
}
// ----

void
pcie_scan_ecam(struct pcie_confspace * base)
{
    volatile struct pcie_confspace * conf = 0;
    struct start_and_limit snl = {PCIE_PMEM_START, PCIE_PMEM_START, PCIE_MEM_START, PCIE_MEM_START};
    int dom = ((u64)base >> 28) & 0x1f;

    printf("scanning ecam %lx\n", base);
    char pfx[8];
    // here the 0000:00:00.0 syntax should mean ecam:bus:dev.fn (but it's not right yet ...)
    sprintf(pfx, "%04x", ((u64)base >> 28) & 0xf);      // 16 ecam spaces?
    int dp = push_pfx(pfx);

    for (int bus = 0; bus < 256; bus++) {
        volatile struct pcie_confspace * conf_bus = PCIE_CONF(base, 0, bus, 0);
        if (conf_bus->vendorId == 0xffff) continue;

        // do we care about header type here?
        if ((conf_bus->headerType & 0x7f) == 1)             // bridge
            printf("%s hdr1 ...\n", Pfx);

        char bpfx[8];
        sprintf(bpfx, ":%02x", bus);
        int pp = push_pfx(bpfx);
        pcie_scan_bus(base, conf_bus, &snl);
        pop_pfx(pp);
        if (conf == 0)
            conf = conf_bus;
    }
    pop_pfx(dp);

    printf("found %d nvme device%s\n", Nnvme_devices, Nnvme_devices > 1 ? "s" : "");
    for (int i = 0; i < Nnvme_devices; i++)
        show_nvme((struct nvme_bar *)Nvme_devices[i].b0);
}


void
pcie_scan(void)
{
    pcie_scan_ecam((struct pcie_confspace *)PCIE_ECAM_BASE);
}
