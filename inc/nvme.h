
// NVM-Express-1_2a.pdf

struct nvme_bar {
    u64 cap;    /* Controller Capabilities */
    u32 vs;     /* Version */
    u32 intms;  /* Interrupt Mask Set */
    u32 intmc;  /* Interrupt Mask Clear */
    u32 cc;     /* Controller Configuration */
    u32 rsvd1;  /* Reserved */
    u32 csts;   /* Controller Status */
    u32 nssr;   /* Subsystem Reset */
    u32 aqa;    /* Admin Queue Attributes */
    u64 asq;    /* Admin SQ Base Address */
    u64 acq;    /* Admin CQ Base Address */
    u32 cmbloc; /* Controller Memory Buffer Location */
    u32 cmbsz;  /* Controller Memory Buffer Size */
};

struct nvme_sqe {
    u8  opcode;
    u8  flags;
    u16 cid;
    u32 nsid;
    u64 rsrvd;
    u64 metadata;
    u64 prp1;
    u64 prp2;
    u32 cdw10;
    u32 cdw11;
    u32 cdw12;
    u32 cdw13;
    u32 cdw14;
    u32 cdw15;
};

struct nvme_cqe {
    u32 result;                 // Used by admin commands to return data
    u32 rsvd;
    u16 sq_head;                // how much of this queue may be reclaimed
    u16 sqid;                   // submission queue that generated this entry
    u16 cid;                    // id of the command which completed 
    u16 status;                 // did the command fail, and if so, why
};

enum {
    NVME_CC_ENABLE              = 1 << 0,
    NVME_CC_CSS_NVM             = 0 << 4,
    NVME_CC_MPS_SHIFT           = 7,
};

#define PGSZ                    4096
#define NVME_CC_MPS             ((LOG((PGSZ) >> 12)) << NVME_CC_MPS_SHIFT)

