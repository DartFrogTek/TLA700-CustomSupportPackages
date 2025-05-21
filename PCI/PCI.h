#ifndef PCI_H
#define PCI_H

#include "..\ISA\stdint.h"
#include "..\ISA\compat.h"
#include <vector>
using namespace std;

/*********************************************************
        Defines
*********************************************************/

// PCI Bus Signals (conventional PCI 2.3)
#define PCI_CLK         0x00000001  // Clock
#define PCI_RST         0x00000002  // Reset (RST#)
#define PCI_FRAME       0x00000004  // FRAME# signal
#define PCI_IRDY        0x00000008  // IRDY# signal (Initiator Ready)
#define PCI_TRDY        0x00000010  // TRDY# signal (Target Ready)
#define PCI_STOP        0x00000020  // STOP# signal
#define PCI_DEVSEL      0x00000040  // DEVSEL# signal (Device Select)
#define PCI_PAR         0x00000080  // PAR signal (Parity)
#define PCI_PERR        0x00000100  // PERR# signal (Parity Error)
#define PCI_SERR        0x00000200  // SERR# signal (System Error)
#define PCI_INTA        0x00000400  // INTA# signal
#define PCI_INTB        0x00000800  // INTB# signal
#define PCI_INTC        0x00001000  // INTC# signal
#define PCI_INTD        0x00002000  // INTD# signal
#define PCI_GNT         0x00004000  // GNT# signal (Bus Grant)
#define PCI_REQ         0x00008000  // REQ# signal (Bus Request)
#define PCI_RESERVED    0x00010000  // Reserved signals
#define PCI_C_BE        0x000F0000  // C/BE# signals (Command/Byte Enable) - 4 bits
#define PCI_IDSEL       0x00100000  // IDSEL signal
#define PCI_LOCK        0x00200000  // LOCK# signal
#define PCI_AD          0xFFC00000  // AD signals (Address/Data) - 10 bits for simplicity

// PCI Commands (C/BE# during address phase)
#define PCI_CMD_INTERRUPT_ACK     0x0
#define PCI_CMD_SPECIAL_CYCLE     0x1
#define PCI_CMD_IO_READ           0x2
#define PCI_CMD_IO_WRITE          0x3
#define PCI_CMD_RESERVED_4        0x4
#define PCI_CMD_RESERVED_5        0x5
#define PCI_CMD_MEM_READ          0x6
#define PCI_CMD_MEM_WRITE         0x7
#define PCI_CMD_RESERVED_8        0x8
#define PCI_CMD_RESERVED_9        0x9
#define PCI_CMD_CONFIG_READ       0xA
#define PCI_CMD_CONFIG_WRITE      0xB
#define PCI_CMD_MEM_READ_MULTIPLE 0xC
#define PCI_CMD_DUAL_ADDR_CYCLE   0xD
#define PCI_CMD_MEM_READ_LINE     0xE
#define PCI_CMD_MEM_WRITE_AND_INV 0xF

// PCI Transaction completion types
enum PCI_COMPLETION {
    PCI_COMP_NORMAL,
    PCI_COMP_MASTER_ABORT,
    PCI_COMP_TARGET_ABORT,
    PCI_COMP_RETRY,
    PCI_COMP_DISCONNECT
};

// PCI burst transfer sizes
enum PCI_BURST_TYPE {
    PCI_BURST_SINGLE,
    PCI_BURST_MULTIPLE,
    PCI_BURST_LINE,
    PCI_BURST_CONTINUOUS
};

/*********************************************************
        TLA Types
*********************************************************/

enum TLA_INFO {
        TLA_INFO_FIRST_SEQUENCE,
        TLA_INFO_LAST_SEQUENCE,
        TLA_INFO_DISPLAY_ATTRIBUTE,
        TLA_INFO_3,
        TLA_INFO_MNEMONICS_WIDTH=5
};

struct lactx;
struct businfo {
        int val0;
        int val4;
        int val8;
        int valc;
        int val10;
        int val14;
        void *val18;
        int val1c;
};

struct modeinfo {
        const char *name;
        const char **options;
        int val1;
        int val2;
};

struct sequence {
        struct sequence *next;
        char *textp;
        uint8_t flags;  // The flags determine the background
                        // bit 0= white (blank), bit 1=grey, bit 2=red, bit3=yellow
        char field_9;
        char field_A;
        char field_B;
        char *text2;
        int field_10;
        int field_14;
        int field_18;
        int field_1C;
        char text[128];
        char text2_buf[8];
};

struct pctx_functable {
        int (*LAGroupValue)(struct lactx *lactx, int seqno, int group);
        void (*rda_free)(void *p);
        void *(*rda_calloc)(int memb, int size);
        int (*LAInfo)(struct lactx *, enum TLA_INFO, int16_t bus);
};

struct pctx {
        struct lactx *lactx;
        struct pctx_functable func;
};

struct groupinfo {
        char *name;
        char field_4;
        char field_5;
        char field_6;
        char field_7;
        uint16_t members;
        uint16_t len;
};

enum MODEINFO {
        MODEINFO_MAX_BUS=0,
        MODEINFO_MAX_GROUP=1,
        MODEINFO_MAX_MODE=2,
        MODEINFO_3=3,
        MODEINFO_GETNAME=4,
        MODEINFO_MAX,
};

struct lafunc {
        int unknown;                               /* 0  */
        char *support_path;                        /* 4  */
        char *support_sep;                         /* 8  */
        char *support_name;                        /* C  */
        char *support_name2;                       /* 10 */
        char *support_ext;                         /* 14 */
        void *(*rda_malloc)(int size);             /* 18 */
        void *(*rda_calloc)(int members, int size);/* 1C */
        void *(*rda_realloc)(void *p, int size);   /* 20 */
        void (*rda_free)(void *p);                 /* 24 */
        void (*LABus)(void);                       /* 28 */
        int  (*LAInfo)(struct lactx *, enum TLA_INFO, int16_t bus);       /* 2C */
        void (*LAError)(struct lactx *, int, char *, ...);                /* 30 */
        void (*LAFindSeq)(void);                   /* 34 */
        void (*LAFormatValue)(void);               /* 38 */
        void (*LAGap)(void);                       /* 3c */
        int (*LAGroupValue)(struct lactx *lactx, int seqno, int group);   /* 40 */
        void (*LAInvalidate)(void);                /* 44 */
        void (*LASeqToText)(void);                 /* 48 */
        void (*LAGroupWidth_)(void);               /* 4c */
        void (*LATimeStamp_ps_)(void);             /* 50 */
        void (*LASysTrigTime_ps_)(void);           /* 54 */
        void (*LABusModTrigTime_ps_)(void);        /* 58 */
        void (*LABusModTimeOffset_ps_)(void);      /* 5c */
        void (*LAGroupInvalidBitMask_)(void);      /* 60 */
        void (*LAContigLongToSeq_)(void);          /* 64 */
        void (*LALongToSeq_)(void);                /* 68 */
        void (*LALongToValidSeq_)(void);           /* 6c */
        void (*LASeqToContigLong_)(void);          /* 70 */
        void (*LASeqToLong_)(void);                /* 74 */
        void (*LASubDisasmLoad)(void);             /* 78 */
        void (*LASubDisasmUnload)(void);           /* 7c */
        void (*LASubDisasmFuncTablePtr_)(void);    /* 80 */
        void (*LAWhichBusMod_)(void);              /* 84 */
        void (*LASeqDisplayFormat_)(void);         /* 88 */
        void (*LAInteractiveUI2_)(void);           /* 8c */
        void (*LAProgAbort_)(void);                /* 90 */
        void (*LATimestamp_ps_ToText)(void);       /* 94 */
        void (*LATimeStampDisplayFormat)(void);    /* 98 */
        void (*LAReferenceTime_ps_)(void);         /* 9c */
        void (*LABusModSysTrigTime_ps_)(void);     /* a0 */
        void (*LABusModFrameOffset_ps_)(void);     /* a4 */
        void (*LABusModTimeToUserAlignedTime_ps_)(void); /* a8 */
        void (*LABusModTrigSample)(void);          /* ac */
        void (*LABusModWallClockStart_)(void);     /* b0 */
        void *field_B4;                            /* b4 */
        void (*LAReferenceTime_ps_2)(void);        /* b8 */
        void (*LASampleStatusBits)(void);          /* bc */
        void (*LASampleStatusBitsType_)(void);
        void (*LAGroupViolationBitMask_)(void);
        void (*LAGroupViolationBitMaskType_)(void);
        void (*LABusModVariantName_)(void);
        void (*LASystemName_)(void);
        void (*LASystemPath_)(void);
        void *field_DC;
};

/*********************************************************
        PCI Types
*********************************************************/

// PCI state machine states
enum PCI_STATES {
    PCI_IDLE,
    PCI_BUS_PARKING,
    PCI_ARB_PHASE,
    PCI_ADDRESS_PHASE,
    PCI_TURNAROUND_PHASE,
    PCI_DATA_PHASE,
    PCI_DUAL_ADDRESS_PHASE,
    PCI_COMPLETION_PHASE,
    PCI_SPECIAL_CYCLE,
    PCI_CONFIG_CYCLE
};

// PCI Address decoding types
enum PCI_ADDR_TYPE {
    PCI_ADDR_MEM,
    PCI_ADDR_IO,
    PCI_ADDR_CONFIG_TYPE0,
    PCI_ADDR_CONFIG_TYPE1
};

// Data structure for PCI transactions
typedef struct TPCIData
{
    // Sequence information
    int sequence_start;          // where this transaction was found
    int sequence_end;            // where the transaction ends
    
    // State machine state
    int state;                   // current state in protocol analysis
    
    // Transaction properties
    uint8_t command;             // PCI command (I/O read/write, Memory read/write, etc.)
    bool is_64bit;               // Is this a 64-bit transaction
    uint8_t completion_type;     // How the transaction completed (normal, abort, etc.)
    
    // Address and data
    uint64_t address;            // 64-bit address (for 32-bit, high 32 bits are 0)
    uint32_t data[16];           // Up to 16 data phases in a burst
    uint8_t byte_enables[16];    // Byte enables for each data phase
    int data_phase_count;        // Number of data phases
    
    // Configuration cycle specific
    uint8_t device_num;          // Device number for config cycles
    uint8_t function_num;        // Function number for config cycles
    bool is_type1_config;        // Is this a type 1 config cycle
    
    // Arbitration
    bool req_asserted;           // REQ# asserted during this transaction
    bool gnt_asserted;           // GNT# asserted during this transaction
    
    // Error conditions
    bool parity_error;           // PERR# asserted
    bool system_error;           // SERR# asserted
    bool master_abort;           // Master abort condition
    bool target_abort;           // Target abort condition
    
    // Burst details
    uint8_t burst_type;          // Single, multiple, line, continuous
    
    // Special signals
    bool lock_asserted;          // LOCK# asserted
    bool has_interrupt;          // Interrupt detected
    uint8_t interrupt_line;      // Which interrupt line was active (A, B, C, D)
    
    // Cache line
    bool is_cache_line;          // Is this a cache line transaction
} TPCIData;

// Data structure for sequence entries
typedef struct TSeqData
{
    int seq_number;
    struct sequence seq_data;
} TSeqData;

typedef vector<TSeqData> TVSeqData;

/*********************************************************
        DLL prototypes
*********************************************************/
extern "C"
{
__declspec(dllexport) struct pctx *ParseReinit(struct pctx *pctx, struct lactx *lactx, struct lafunc *func);
__declspec(dllexport) int ParseFinish(struct pctx *pctx);
__declspec(dllexport) int ParseInfo(struct pctx *pctx, unsigned int request);
__declspec(dllexport) int ParseMarkMenu(struct pctx *, int, int, int, int);
__declspec(dllexport) int ParseMarkGet(struct pctx *pctx, int seq);
__declspec(dllexport) int ParseMarkSet(struct pctx *pctx, int seq, int);
__declspec(dllexport) int ParseMarkNext(struct pctx *pctx, int seq, int);
__declspec(dllexport) int ParseModeGetPut(struct pctx *pctx, int mode, int, int request);
__declspec(dllexport) struct sequence *ParseSeq(struct pctx *, int seq);
__declspec(dllexport) struct businfo *ParseBusInfo(struct pctx *, uint16_t bus);
__declspec(dllexport) struct modeinfo *ParseModeInfo(struct pctx *pctx, uint16_t mode);
__declspec(dllexport) struct groupinfo *ParseGroupInfo(struct pctx *pctx, uint16_t group);
__declspec(dllexport) int ParseDisasmReinit(struct pctx *, int request);
}

#endif // PCI_H