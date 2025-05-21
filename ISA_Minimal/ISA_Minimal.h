#ifndef ISA_MINIMAL_H
#define ISA_MINIMAL_H

#include "..\ISA\stdint.h"
#include "..\ISA\compat.h"
#include <vector>
using namespace std;

/*********************************************************
        Defines
*********************************************************/

// Feature detection flags
#define ISA_FEATURE_16BIT        0x01    // Support for 16-bit data bus
#define ISA_FEATURE_20BIT_ADDR   0x02    // Support for 20-bit address bus
#define ISA_FEATURE_24BIT_ADDR   0x04    // Support for 24-bit address bus
#define ISA_FEATURE_DMA          0x08    // Support for DMA operations
#define ISA_FEATURE_IRQ          0x10    // Support for IRQ signals

// ISA Bus Core Signals (Required)
#define ISA_BCLK        0x00000001  // Bus Clock
#define ISA_ALE         0x00000002  // Address Latch Enable
#define ISA_IOR         0x00000004  // I/O Read (active low)
#define ISA_IOW         0x00000008  // I/O Write (active low)
#define ISA_MEMR        0x00000010  // Memory Read (active low)
#define ISA_MEMW        0x00000020  // Memory Write (active low)

// ISA Bus Optional Signals
#define ISA_REFRESH     0x00000040  // Memory Refresh (active low)
#define ISA_SBHE        0x00000080  // System Bus High Enable (active low)
#define ISA_IOCHRDY     0x00000100  // I/O Channel Ready (active high)
#define ISA_AEN         0x00000200  // Address Enable (active high)
#define ISA_RESET       0x00000400  // System Reset (active high)
#define ISA_OSC         0x00000800  // Oscillator (14.31818 MHz)
#define ISA_IOCHK       0x00001000  // I/O Channel Check (active low)

// DMA Signals
#define ISA_MASTER      0x00010000  // Bus Master (active low)
#define ISA_DRQ         0x00020000  // DMA Request (any)
#define ISA_DACK        0x00040000  // DMA Acknowledge (any)
#define ISA_TC          0x00080000  // Terminal Count (active high)

// Optional signal masks
#define ISA_CORE_MASK   (ISA_BCLK | ISA_ALE | ISA_IOR | ISA_IOW | ISA_MEMR | ISA_MEMW)
#define ISA_OPT_MASK    (ISA_REFRESH | ISA_SBHE | ISA_IOCHRDY | ISA_AEN | ISA_RESET)

// Address Lines
#define ISA_ADDR_MASK   0x000FFFFF  // Up to 20-bit address

// Data Lines
#define ISA_DATA_MASK   0x0000FFFF  // 16-bit data bus

// ISA state machine states
enum ISA_STATE {
    ISA_STATE_IDLE,
    ISA_STATE_T1,         // First clock cycle of bus cycle
    ISA_STATE_T2,         // Second clock cycle of bus cycle - data transfer
    ISA_STATE_TW,         // Wait states
    ISA_STATE_T3,         // Third and final clock cycle of bus cycle
    ISA_STATE_REFRESH     // Memory refresh cycle
};

// ISA transaction types
enum ISA_TRANSACTION_TYPE {
    ISA_TRANS_NONE,
    ISA_TRANS_IO_READ_BYTE,    // 8-bit I/O read
    ISA_TRANS_IO_READ_WORD,    // 16-bit I/O read
    ISA_TRANS_IO_WRITE_BYTE,   // 8-bit I/O write
    ISA_TRANS_IO_WRITE_WORD,   // 16-bit I/O write
    ISA_TRANS_MEM_READ_BYTE,   // 8-bit memory read
    ISA_TRANS_MEM_READ_WORD,   // 16-bit memory read
    ISA_TRANS_MEM_WRITE_BYTE,  // 8-bit memory write
    ISA_TRANS_MEM_WRITE_WORD,  // 16-bit memory write
    ISA_TRANS_DMA_READ_BYTE,   // 8-bit DMA read
    ISA_TRANS_DMA_READ_WORD,   // 16-bit DMA read
    ISA_TRANS_DMA_WRITE_BYTE,  // 8-bit DMA write
    ISA_TRANS_DMA_WRITE_WORD,  // 16-bit DMA write
    ISA_TRANS_REFRESH,         // Memory refresh cycle
    ISA_TRANS_ERROR            // Error condition
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
    uint8_t flags;      // The flags determine the background
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
    int unknown;                                        /* 0  */
    char *support_path;                                 /* 4  */
    char *support_sep;                                  /* 8  */
    char *support_name;                                 /* C  */
    char *support_name2;                                /* 10 */
    char *support_ext;                                  /* 14 */
    void *(*rda_malloc)(int size);                      /* 18 */
    void *(*rda_calloc)(int members, int size);         /* 1C */
    void *(*rda_realloc)(void *p, int size);            /* 20 */
    void (*rda_free)(void *p);                          /* 24 */
    void (*LABus)(void);                                /* 28 */
    int  (*LAInfo)(struct lactx *, enum TLA_INFO, int16_t bus);         /* 2C */
    void (*LAError)(struct lactx *, int, char *, ...);                  /* 30 */
    void (*LAFindSeq)(void);                            /* 34 */
    void (*LAFormatValue)(void);                        /* 38 */
    void (*LAGap)(void);                                /* 3c */
    int (*LAGroupValue)(struct lactx *lactx, int seqno, int group);     /* 40 */
    void (*LAInvalidate)(void);                         /* 44 */
    void (*LASeqToText)(void);                          /* 48 */
    void (*LAGroupWidth_)(void);                        /* 4c */
    void (*LATimeStamp_ps_)(void);                      /* 50 */
    void (*LASysTrigTime_ps_)(void);                    /* 54 */
    void (*LABusModTrigTime_ps_)(void);                 /* 58 */
    void (*LABusModTimeOffset_ps_)(void);               /* 5c */
    void (*LAGroupInvalidBitMask_)(void);               /* 60 */
    void (*LAContigLongToSeq_)(void);                   /* 64 */
    void (*LALongToSeq_)(void);                         /* 68 */
    void (*LALongToValidSeq_)(void);                    /* 6c */
    void (*LASeqToContigLong_)(void);                   /* 70 */
    void (*LASeqToLong_)(void);                         /* 74 */
    void (*LASubDisasmLoad)(void);                      /* 78 */
    void (*LASubDisasmUnload)(void);                    /* 7c */
    void (*LASubDisasmFuncTablePtr_)(void);             /* 80 */
    void (*LAWhichBusMod_)(void);                       /* 84 */
    void (*LASeqDisplayFormat_)(void);                  /* 88 */
    void (*LAInteractiveUI2_)(void);                    /* 8c */
    void (*LAProgAbort_)(void);                         /* 90 */
    void (*LATimestamp_ps_ToText)(void);                /* 94 */
    void (*LATimeStampDisplayFormat)(void);             /* 98 */
    void (*LAReferenceTime_ps_)(void);                  /* 9c */
    void (*LABusModSysTrigTime_ps_)(void);              /* a0 */
    void (*LABusModFrameOffset_ps_)(void);              /* a4 */
    void (*LABusModTimeToUserAlignedTime_ps_)(void);    /* a8 */
    void (*LABusModTrigSample)(void);                   /* ac */
    void (*LABusModWallClockStart_)(void);              /* b0 */
    void *field_B4;                                     /* b4 */
    void (*LAReferenceTime_ps_2)(void);                 /* b8 */
    void (*LASampleStatusBits)(void);                   /* bc */
    void (*LASampleStatusBitsType_)(void);              /* c0 */
    void (*LAGroupViolationBitMask_)(void);             /* c4 */
    void (*LAGroupViolationBitMaskType_)(void);         /* c8 */
    void (*LABusModVariantName_)(void);                 /* cc */
    void (*LASystemName_)(void);                        /* d0 */
    void (*LASystemPath_)(void);                        /* d4 */
    void *field_DC;                                     /* d8 */
};

// Data structures
typedef struct TISAData
{
    // Transaction information
    int sequence;             // Starting sequence number
    int last_sequence;        // Ending sequence number
    int state;                // Current state in ISA state machine
    int transaction_type;     // Type of ISA transaction
    uint32_t address;         // ISA bus address (up to 20 bits)
    uint16_t data;            // ISA bus data (8 or 16 bits)
    
    // Signal state tracking
    int ior_active;           // IOR# is active (low)
    int iow_active;           // IOW# is active (low)
    int memr_active;          // MEMR# is active (low)
    int memw_active;          // MEMW# is active (low)
    int refresh_active;       // REFRESH# is active (low)
    int sbhe_active;          // SBHE# is active (low)
    int iochrdy_active;       // IOCHRDY is active (high)
    int aen_active;           // AEN is active (high)
    int master_active;        // MASTER# is active (low)
    int tc_active;            // TC is active (high)
    
    // Transaction attributes
    int wait_states;          // Number of wait states in current transaction
    int is_16bit;             // Is this a 16-bit transaction?
    
    // Address decoding
    int use_io_space;         // I/O space (vs memory space)
    
    // Timing and error tracking
    int bus_timing_cycles;    // Bus cycles for timing verification
    int timed_out;            // Transaction timed out
    int protocol_error;       // Protocol violation detected
    char error_message[64];   // Error message if applicable
    
} TISAData;

// ISA address and data structure for tracking component states
typedef struct TISABusData {
    int addr_latch_state;     // 0 = not latched, 1 = partially latched, 2 = fully latched
    uint32_t partial_addr;    // Address accumulator during latching
    uint32_t latched_addr;    // Fully latched address
    uint16_t pending_data;    // Data accumulator during data phase
    uint16_t data;            // Completed data transfer
    int addr_valid;           // Address has been validated
    int data_valid;           // Data has been validated
    int bus_width;            // Current bus width (8 or 16 bits)
} TISABusData;

typedef struct TSeqData
{
    int seq_number;
    struct sequence seq_data;
} TSeqData;

typedef vector<TSeqData> TVSeqData;

typedef struct TISAFeatureConfig {
    int enabled_features;     // Bitmap of enabled features
    int addr_width;           // Address width in bits (16, 20, or 24)
    int data_width;           // Data width in bits (8 or 16)
    int addr_group;           // Group number for address signals
    int data_group;           // Group number for data signals
    int control_group;        // Group number for control signals
} TISAFeatureConfig;

// Configuration structure for TLA 7L2
typedef struct T7L2Config {
    int control_group;        // Group number for control signals (default 0)
    int address_group;        // Group number for address signals (default 1)
    int data_group;           // Group number for data signals (default 2)
    int addr_width;           // Address width (16, 20, or 24 bits)
    int data_width;           // Data width (8 or 16 bits)
    int support_dma;          // Support DMA signals
    int support_irq;          // Support IRQ signals
    int support_16bit;        // Support 16-bit transfers
} T7L2Config;

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
__declspec(dllexport) int ParseExtInfo_(struct pctx *pctx);
}

#endif // ISA_Minimal