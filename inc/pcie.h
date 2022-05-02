/*
 *  PCI Express  ECAM
 *
 *  reference: include/uapi/linux/pci_regs.h
 */

#define PCIE_CONF_SIZE      0x1000
#define PCIE_CONF_SHIFT     12
#define PCIE_CONF_BDF_MASK  0xffff

#define PCIE_CONF_SIZE_NOF  (PCIE_CONF_SIZE << 3)       // PCIE_CONF_SIZE without functions

// bus/device/function fields in headers   pg 68
#define PCIE_NR_BUS         256
#define PCIE_NR_DEV         32
#define PCIE_NR_FUNC        8
#define PCIE_DEV_SHIFT      5
#define PCIE_FUNC_SHIFT     3
#define BUS(c)              ((((u64)(c)) >> (PCIE_CONF_SHIFT + PCIE_FUNC_SHIFT + PCIE_DEV_SHIFT)) & (PCIE_NR_BUS - 1))
#define DEV(c)              ((((u64)(c)) >> (PCIE_CONF_SHIFT + PCIE_FUNC_SHIFT)) & (PCIE_NR_DEV - 1))

#define PCI_CONF_BAR_IO         0x01
#define PCI_CONF_BAR_BELOW1MB   0x02
#define PCI_CONF_BAR_64BIT      0x04
#define PCI_CONF_BAR_PREFETCH   0x08
#define PCI_CONF_BAR_MASK       0xfffffffffffffff0ull

#define PCI_CMD_IO              0x001
#define PCI_CMD_MEM             0x002           // undocumented in the PCIe v3.0 spec?
#define PCI_CMD_BUS_MASTER      0x004
#define PCI_CMD_PARITY          0x040
#define PCI_CMD_SERR            0x100
#define PCI_CMD_IRQ_DISABLE     0x400
#define PCI_CMD_DEV             (PCI_CMD_IO | PCI_CMD_MEM | PCI_CMD_BUS_MASTER | PCI_CMD_PARITY | PCI_CMD_SERR)
#define PCI_CMD_BUS             (PCI_CMD_IO | PCI_CMD_MEM | PCI_CMD_BUS_MASTER | PCI_CMD_IRQ_DISABLE | PCI_CMD_SERR)
// PCI_CMD_IRQ_DISABLE ?

#define PCI_BRCTL_SECBUS_RESET  0x40

struct pcie_confspace {
    u16 vendorId;
    u16 deviceId;
    u16 command;                        // pg 589
    u16 status;                         // pg 591
    u8  revisionId;
    u8  progIf;
    u8  subclass;
    u8  classCode;
    u8  cacheLineSize;
    u8  latTimer;
    u8  headerType;
    u8  bist;
    u32 bar[2];
    union {
        struct pcie_ep_config0 {         // pg 595
            u32 bar2;                   // note that indexing bar[n] out-of-bounds
            u32 bar3;                   // correctly accesses these bars ...
            u32 bar4;
            u32 bar5;
            u32 cardbusCIS;
            u16 subsysVendorId;
            u16 subsysId;
            u32 expansionRomAddr;
        } ep;
        struct pcie_rp_config1 {         // pg 597
            u8  priBus;
            u8  secBus;
            u8  subBus;
            u8  secLat;
            u8  ioBase;
            u8  ioLimit;
            u16 secStatus;
            u16 memBase;
            u16 memLimit;
            u16 pmemBase;
            u16 pmemLimit;
            u32 pmemBaseH;
            u32 pmemLimitH;
            u16 ioBaseH;
            u16 ioLimitH;
        } rp;
    };
    u8  capPtr;
    u8  reserved0;
    u16 reserved1;
    u32 rp_expRomAddr;
    u8  irqLine;
    u8  irqPin;
    union {
        struct pcie_ep_config1 {
            u8  minGrant;
            u8  maxLatency;
        } epx;
        u16 bridgeCtl;                  // pg 600
    };
}; // __attribute__ ((__packed__));

struct pcie_capability {
    u8  capId;
    u8  next;
    u16 capReg;
    u32 devCap;                 // v3.0 pg 608
    u16 devControl;             // v3.0 pg 613
    u16 devStatus;              // v3.0 pg 620
    u32 linkCap;                // v3.0 pg 622
    u16 linkControl;            // v3.0 pg 627
    u16 linkStatus;             // v3.0 pg 635
    u32 slotCap;                // v3.0 pg 638
    u16 slotControl;            // v3.0 pg 640
    u16 slotStatus;             // v3.0 pg 644
    u16 rootControl;            // v3.0 pg 646
    u16 rootCap;                // v3.0 pg 647
    u32 rootStatus;             // v3.0 pg 648
    // dev2
    // link2
    // slot2
};


// PCIe base spec v3.0  pg 635
#define PCIE_LINKSTAT_LINK_ACTIVE           0x2000
#define PCIE_LINKSTAT_TRAINING              0x0800
#define PCIE_LINKSTAT_WIDTH                 0x03f0
#define PCIE_LINKSTAT_SPEED                 0x000f

// PCIe base spec v3.0  pg 638
#define PCIE_SLOTCAP_HOTPLUG_CAP            0x00040

// PCIe base spec v3.0  pg 640
#define PCIE_SLOTCTL_ENABLE_PRESENCE_CHANGE 0x008

// PCIe base spec v3.0  pg 644
#define PCIE_SLOTSTAT_PRESENCE_CHANGE       0x008
#define PCIE_SLOTSTAT_PRESENCE_STATE        0x040
#define PCIE_SLOTSTAT_DATALINK_CHANGE       0x100

// pg 524  Data Link Layer State Changed Events


char * cap_names[] = {
    "",      " pm",    " agp",   " vpd",
    " slot", " msi",   " chswp", " pcix",
    " ht",   " vndr",  " dbg",   " ccrc",
    " shpc", " ssvid", " agp3",  " secdev",
    " exp",  " msi-x", " sata",  " af",
    " ea"
};

