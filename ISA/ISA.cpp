#include <vector>
using namespace std;
#include "ISA.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <windows.h>
#include <time.h>
#include <errno.h>
#include <string.h>
//#define WITH_DEBUG

/*********************************************************
        Globals signal setup
*********************************************************/
static FILE *logfile = NULL;
const char *modeinfo_names[MODEINFO_MAX] = {
    "MAX_BUS",
    "MAX_GROUP",
    "MAX_MODE",
    "3",
    "GETNAME"
};

const int bustable[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// Define groups (match the number of groups to the channel definitions in ISA.tla)
const struct groupinfo groupinfo[] = { 
    { "ISA_Control", 0, 0, 0, 0, 0x00003FFF, 0 },  // ISA control signals
    { "ISA_Addr", 0, 0, 0, 0, (unsigned short)ISA_ADDR_MASK, 0 },  // Address lines
    { "ISA_Data", 0, 0, 0, 0, (unsigned short)ISA_DATA_MASK, 0 },  // Data lines
    { "ISA_DMA", 0, 0, 0, 0, (unsigned short)0x7FFFFFFF, 0 },      // DMA signals
    { "ISA_IRQ", 0, 0, 0, 0, 0x00000FFF, 0 },      // IRQ signals
    { "Traffic", 0, 0, 2, 1, 0x80, sizeof("Traffic")-1 } 
};

const struct businfo businfo[] = { { 0, 0, 0, 0, 0, 0x20000, NULL, 0 } };

// Define settings
const char *addr_width[] = { "16-bit", "20-bit", "24-bit", NULL };
const char *bus_speed[] = { "4.77 MHz", "6 MHz", "8 MHz", "10 MHz", "12 MHz", NULL };
const char *dma_support[] = { "Disabled", "Enabled", NULL };
const char *refresh_support[] = { "Disabled", "Enabled", NULL };
const char *irq_support[] = { "Disabled", "Enabled", NULL };
const char *timing_mode[] = { "Fast Mode", "Normal Mode", "Slow Mode", "AT Mode", "ISA Mode", NULL };
const char *error_detection[] = { "Basic", "Advanced", NULL };

const struct modeinfo modeinfo[] = { 
    { "ADDR_WIDTH", addr_width, 0, 2 },
    { "BUS_SPEED", bus_speed, 2, 4 },
    { "DMA_SUPPORT", dma_support, 1, 1 },
    { "REFRESH_SUPPORT", refresh_support, 1, 1 },
    { "IRQ_SUPPORT", irq_support, 1, 1 },
    { "TIMING_MODE", timing_mode, 3, 4 },
    { "ERROR_DETECTION", error_detection, 1, 1 }
};

// Names for the transaction types (for better readability)
const char* transaction_names[] = {
    "None",
    "I/O Read (8-bit)",
    "I/O Read (16-bit)",
    "I/O Write (8-bit)",
    "I/O Write (16-bit)",
    "Memory Read (8-bit)",
    "Memory Read (16-bit)",
    "Memory Write (8-bit)",
    "Memory Write (16-bit)",
    "DMA Read (8-bit)",
    "DMA Read (16-bit)",
    "DMA Write (8-bit)",
    "DMA Write (16-bit)",
    "Memory Refresh",
    "Error"
};

/*********************************************************
        ISA analysis data
*********************************************************/
static TISAData ISAData[1];       // Active transaction data
static TISABusData ISABusData[1]; // Bus state tracking
static int set_addr_width;        // Address width setting
static int set_bus_speed;         // Bus speed setting
static int set_dma_support;       // DMA support setting
static int set_refresh_support;   // Refresh support setting
static int set_irq_support;       // IRQ support setting
static int set_timing_mode;       // Timing mode setting
static int set_error_detection;   // Error detection setting
static int processing_done;
TVSeqData SeqDataVector;          // Vector with analysis results

/*********************************************************
        Helpers
*********************************************************/
#ifdef WITH_DEBUG
static void LogDebug(struct pctx *pctx, int level, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    if (logfile)
    {
        fprintf(logfile, "%s\n", buf);
        fflush(logfile);
    }
    
    OutputDebugString(buf);
}
#endif

// Helper function to get address width in bits based on setting
static int GetAddressWidthBits()
{
    switch (set_addr_width)
    {
        case 0: return 16;
        case 1: return 20;
        case 2: return 24;
        default: return 16;
    }
}

// Helper function to get bus clock period in nanoseconds based on setting
static double GetClockPeriodNS()
{
    switch (set_bus_speed)
    {
        case 0: return 209.64;  // 4.77 MHz = 209.64 ns
        case 1: return 166.67;  // 6 MHz = 166.67 ns
        case 2: return 125.0;   // 8 MHz = 125 ns
        case 3: return 100.0;   // 10 MHz = 100 ns
        case 4: return 83.33;   // 12 MHz = 83.33 ns
        default: return 125.0;  // Default to 8 MHz
    }
}

// Helper function to format address based on address width
static void FormatAddress(char* buf, size_t buf_size, uint32_t addr)
{
    switch (set_addr_width)
    {
        case 0: // 16-bit
            snprintf(buf, buf_size, "0x%04X", addr & 0xFFFF);
            break;
        case 1: // 20-bit
            snprintf(buf, buf_size, "0x%05X", addr & 0xFFFFF);
            break;
        case 2: // 24-bit
            snprintf(buf, buf_size, "0x%06X", addr & 0xFFFFFF);
            break;
        default:
            snprintf(buf, buf_size, "0x%04X", addr & 0xFFFF);
            break;
    }
}

// Helper to check if the given value is active low (normally high)
static int IsActiveLow(uint32_t signal, uint32_t value)
{
    return (value & signal) == 0 ? MY_TRUE : MY_FALSE;
}

// Helper to check if the given value is active high (normally low)
static int IsActiveHigh(uint32_t signal, uint32_t value)
{
    return (value & signal) != 0 ? MY_TRUE : MY_FALSE;
}

// Helper function to identify active DMA channel from DACKn signals
static int IdentifyActiveDMAChannel(uint32_t value)
{
    if (IsActiveLow(ISA_DACK0, value)) return 0;
    if (IsActiveLow(ISA_DACK1, value)) return 1;
    if (IsActiveLow(ISA_DACK2, value)) return 2;
    if (IsActiveLow(ISA_DACK3, value)) return 3;
    if (IsActiveLow(ISA_DACK5, value)) return 5;
    if (IsActiveLow(ISA_DACK6, value)) return 6;
    if (IsActiveLow(ISA_DACK7, value)) return 7;
    return -1; // No DMA channel active
}

// Helper function to identify active IRQ line from IRQn signals
static int IdentifyActiveIRQLine(uint32_t value)
{
    if (IsActiveHigh(ISA_IRQ2, value)) return 2;
    if (IsActiveHigh(ISA_IRQ3, value)) return 3;
    if (IsActiveHigh(ISA_IRQ4, value)) return 4;
    if (IsActiveHigh(ISA_IRQ5, value)) return 5;
    if (IsActiveHigh(ISA_IRQ6, value)) return 6;
    if (IsActiveHigh(ISA_IRQ7, value)) return 7;
    if (IsActiveHigh(ISA_IRQ9, value)) return 9;
    if (IsActiveHigh(ISA_IRQ10, value)) return 10;
    if (IsActiveHigh(ISA_IRQ11, value)) return 11;
    if (IsActiveHigh(ISA_IRQ12, value)) return 12;
    if (IsActiveHigh(ISA_IRQ14, value)) return 14;
    if (IsActiveHigh(ISA_IRQ15, value)) return 15;
    return -1; // No IRQ line active
}

// Helper function to create a new sequence data entry
static void CreateSequenceEntry(int seq_number, int trans_type, bool error_flag, const char* format, ...)
{
    TSeqData SeqData;
    memset(&SeqData, 0, sizeof(SeqData));
    
    va_list args;
    va_start(args, format);
    vsnprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), format, args);
    va_end(args);
    
    // Set flags based on transaction type and error status
    if (error_flag)
    {
        SeqData.seq_data.flags = 4;  // Red background for errors
    }
    else if (trans_type == ISA_TRANS_REFRESH)
    {
        SeqData.seq_data.flags = 2;  // Grey background for refresh cycles
    }
    else if (trans_type >= ISA_TRANS_DMA_READ_BYTE && trans_type <= ISA_TRANS_DMA_WRITE_WORD)
    {
        SeqData.seq_data.flags = 8;  // Yellow background for DMA
    }
    else
    {
        SeqData.seq_data.flags = 1;  // Normal white background
    }
    
    SeqData.seq_number = seq_number;
    SeqDataVector.push_back(SeqData);
    
    LogDebug(NULL, 0, "Created sequence: %s", SeqData.seq_data.text);
}

// Helper function to determine if a transaction is 16-bit
static int Is16BitTransaction(int trans_type)
{
    switch (trans_type)
    {
        case ISA_TRANS_IO_READ_WORD:
        case ISA_TRANS_IO_WRITE_WORD:
        case ISA_TRANS_MEM_READ_WORD:
        case ISA_TRANS_MEM_WRITE_WORD:
        case ISA_TRANS_DMA_READ_WORD:
        case ISA_TRANS_DMA_WRITE_WORD:
            return MY_TRUE;
        default:
            return MY_FALSE;
    }
}

/*********************************************************
        DLL functions
*********************************************************/
struct pctx *ParseReinit(struct pctx *pctx, struct lactx *lactx, struct lafunc *func)
{
    // Called upon DLL startup and refreshing the data
    struct pctx *ret;
    SeqDataVector.clear();
    processing_done = 0;
    
    // Check if already initialized
    if (pctx != NULL)
    {
        return pctx;    // already initialized -> exit
    }
    
    if (!(ret = (struct pctx*) func->rda_calloc(1, sizeof(struct pctx))))
    {
        func->LAError(0, 9, "Out of Memory");
        return NULL;
    }
    
    ret->lactx = lactx;
    ret->func.LAGroupValue = func->LAGroupValue;
    ret->func.rda_calloc = func->rda_calloc;
    ret->func.rda_free = func->rda_free;
    ret->func.LAInfo = func->LAInfo;
    
    // default settings
    set_addr_width = 0;         // 16-bit
    set_bus_speed = 2;          // 8 MHz
    set_dma_support = 1;        // DMA enabled
    set_refresh_support = 1;    // Refresh enabled
    set_irq_support = 1;        // IRQ enabled
    set_timing_mode = 3;        // AT Mode
    set_error_detection = 1;    // Advanced error detection
    
    SeqDataVector.clear();
    processing_done = 0;
    
    #ifdef WITH_DEBUG
    // Open debug log file
    if (!logfile)
    {
        logfile = fopen("isa_debug.log", "w");
    }
#endif
    
    LogDebug(ret, 0, "ISA Protocol Analyzer Initialization Completed");
    return ret;
}

int ParseExtInfo_(struct pctx *pctx)
{
    LogDebug(pctx, 0, "%s", "ParseExtInfo_");
    return 0;
}

int ParseFinish(struct pctx *pctx)
{
    LogDebug(pctx, 0, "%s", "ParseFinish");
    SeqDataVector.clear();
    
#ifdef WITH_DEBUG
    if (logfile)
    {
        fclose(logfile);
        logfile = NULL;
    }
#endif
    
    pctx->func.rda_free(pctx);
    return 0;
}

struct sequence *ParseSeq(struct pctx *pctx, int initseq)
{
    // TSeqData SeqData;        // array to hold the new vector element
    TVSeqData::iterator it;
    struct sequence *seqinfo = NULL;
    
    if (pctx == NULL)
    {
        LogDebug(pctx, 0, "pctx NULL");
        return NULL;
    }
    
    if (processing_done == 0)
    {
        processing_done = 1;
        
        // Get the sequence range
        int firstseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_FIRST_SEQUENCE, -1);
        int lastseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_LAST_SEQUENCE, -1);
        LogDebug(pctx, 0, "Processing sequences: initseq=%d, firstseq=%d, lastseq=%d", 
                 initseq, firstseq, lastseq);
        
        // Initialize ISA data structures
        memset(ISAData, 0, sizeof(ISAData));
        memset(ISABusData, 0, sizeof(ISABusData));
        
        ISAData[0].state = ISA_STATE_IDLE;
        ISAData[0].transaction_type = ISA_TRANS_NONE;
        ISAData[0].active_dma_channel = -1;
        ISAData[0].active_irq_line = -1;
        
        // Previous signal states for edge detection
        uint32_t prev_ctrl_signals = 0;
        uint32_t prev_dma_signals = 0;
        uint32_t prev_irq_signals = 0;
        uint32_t prev_address = 0;
        uint32_t prev_data = 0;
        
        int bclk_cycles = 0; // Counter for BCLK cycles
        
        // Now loop through all the samples
        for (int seq = firstseq; seq <= lastseq; seq++)
        {
            // Get signal values for each group
            uint32_t ctrl_signals = pctx->func.LAGroupValue(pctx->lactx, seq, 0); // Control signals
            uint32_t address = pctx->func.LAGroupValue(pctx->lactx, seq, 1);      // Address bus
            uint16_t data = pctx->func.LAGroupValue(pctx->lactx, seq, 2);         // Data bus
            uint32_t dma_signals = pctx->func.LAGroupValue(pctx->lactx, seq, 3);  // DMA signals
            uint32_t irq_signals = pctx->func.LAGroupValue(pctx->lactx, seq, 4);  // IRQ signals
            
            LogDebug(pctx, 5, "Seq: %d, Ctrl: 0x%08X, Addr: 0x%08X, Data: 0x%04X, DMA: 0x%08X, IRQ: 0x%08X", 
                     seq, ctrl_signals, address, data, dma_signals, irq_signals);
            
            // Decode control signals
            bool bclk = IsActiveHigh(ISA_BCLK, ctrl_signals) ? true : false;
            bool ale = IsActiveHigh(ISA_ALE, ctrl_signals) ? true : false;
            bool ior = IsActiveLow(ISA_IOR, ctrl_signals) ? true : false;
            bool iow = IsActiveLow(ISA_IOW, ctrl_signals) ? true : false;
            bool memr = IsActiveLow(ISA_MEMR, ctrl_signals) ? true : false;
            bool memw = IsActiveLow(ISA_MEMW, ctrl_signals) ? true : false;
            bool refresh = IsActiveLow(ISA_REFRESH, ctrl_signals) ? true : false;
            bool master = IsActiveLow(ISA_MASTER, ctrl_signals) ? true : false;
            bool sbhe = IsActiveLow(ISA_SBHE, ctrl_signals) ? true : false;
            bool iochrdy = IsActiveHigh(ISA_IOCHRDY, ctrl_signals) ? true : false;
            bool aen = IsActiveHigh(ISA_AEN, ctrl_signals) ? true : false;
            bool iochk = IsActiveLow(ISA_IOCHK, ctrl_signals) ? true : false;
            bool reset = IsActiveHigh(ISA_RESET, ctrl_signals) ? true : false;
            
            // Decode DMA signals
            bool tc = IsActiveHigh(ISA_TC, dma_signals) ? true : false;
            int active_dma_channel = IdentifyActiveDMAChannel(dma_signals);
            
            // Decode IRQ signals
            int active_irq_line = IdentifyActiveIRQLine(irq_signals);
            
            // Edge detection
            bool bclk_rising_edge = bclk && !IsActiveHigh(ISA_BCLK, prev_ctrl_signals);
            bool bclk_falling_edge = !bclk && IsActiveHigh(ISA_BCLK, prev_ctrl_signals);
            bool ale_rising_edge = ale && !IsActiveHigh(ISA_ALE, prev_ctrl_signals);
            bool ale_falling_edge = !ale && IsActiveHigh(ISA_ALE, prev_ctrl_signals);
            
            bool ior_falling_edge = ior && !IsActiveLow(ISA_IOR, prev_ctrl_signals);
            bool iow_falling_edge = iow && !IsActiveLow(ISA_IOW, prev_ctrl_signals);
            bool memr_falling_edge = memr && !IsActiveLow(ISA_MEMR, prev_ctrl_signals);
            bool memw_falling_edge = memw && !IsActiveLow(ISA_MEMW, prev_ctrl_signals);
            bool refresh_falling_edge = refresh && !IsActiveLow(ISA_REFRESH, prev_ctrl_signals);
            
            bool ior_rising_edge = !ior && IsActiveLow(ISA_IOR, prev_ctrl_signals);
            bool iow_rising_edge = !iow && IsActiveLow(ISA_IOW, prev_ctrl_signals);
            bool memr_rising_edge = !memr && IsActiveLow(ISA_MEMR, prev_ctrl_signals);
            bool memw_rising_edge = !memw && IsActiveLow(ISA_MEMW, prev_ctrl_signals);
            bool refresh_rising_edge = !refresh && IsActiveLow(ISA_REFRESH, prev_ctrl_signals);
            
            // Track BCLK cycles
            if (bclk_rising_edge)
            {
                bclk_cycles++;
                LogDebug(pctx, 7, "BCLK Rising Edge: cycle %d", bclk_cycles);
            }
            
            // ISA bus state machine
            switch (ISAData[0].state)
            {
                case ISA_STATE_IDLE:
                    if (reset)
                    {
                        // System reset detected
                        LogDebug(pctx, 0, "SYSTEM RESET detected");
                        CreateSequenceEntry(seq, ISA_TRANS_NONE, false, "SYSTEM RESET");
                        continue; // Skip further processing during reset
                    }
                    
                    // Initialize transaction data
                    if (ale_rising_edge || ior_falling_edge || iow_falling_edge || 
                        memr_falling_edge || memw_falling_edge || refresh_falling_edge)
                    {
                        LogDebug(pctx, 1, "Starting new transaction");
                        
                        // Initialize new transaction
                        ISAData[0].sequence = seq;
                        ISAData[0].last_sequence = seq;
                        ISAData[0].state = ISA_STATE_T1;
                        ISAData[0].wait_states = 0;
                        ISAData[0].bus_timing_cycles = 0;
                        ISAData[0].is_16bit = sbhe;
                        ISAData[0].start_time_ps = 0; // Would use timestamp here if available
                        ISAData[0].timed_out = false;
                        ISAData[0].protocol_error = false;
                        
                        // Track active command signals
                        ISAData[0].ior_active = ior;
                        ISAData[0].iow_active = iow;
                        ISAData[0].memr_active = memr;
                        ISAData[0].memw_active = memw;
                        ISAData[0].refresh_active = refresh;
                        ISAData[0].master_active = master;
                        ISAData[0].sbhe_active = sbhe;
                        ISAData[0].iochrdy_active = iochrdy;
                        ISAData[0].aen_active = aen;
                        
                        // Capture address (first part from address lines)
                        ISABusData[0].addr_latch_state = 1;
                        ISABusData[0].partial_addr = address & ISA_ADDR_MASK;
                        ISABusData[0].addr_valid = false;
                        ISABusData[0].data_valid = false;
                        
                        // Determine if this is a DMA cycle
                        if (active_dma_channel != -1 && set_dma_support)
                        {
                            ISAData[0].state = ISA_STATE_DMA_ACTIVE;
                            ISAData[0].active_dma_channel = active_dma_channel;
                            ISAData[0].tc_active = tc;
                            LogDebug(pctx, 1, "DMA cycle for channel %d detected", active_dma_channel);
                        }
                        // Check for refresh cycle
                        else if (refresh && set_refresh_support)
                        {
                            ISAData[0].state = ISA_STATE_REFRESH;
                            ISAData[0].transaction_type = ISA_TRANS_REFRESH;
                            LogDebug(pctx, 1, "Memory refresh cycle detected");
                        }
                        // Normal bus cycle
                        else
                        {
                            // Determine the transaction type
                            if (ior)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_IO_READ_WORD : ISA_TRANS_IO_READ_BYTE;
                                ISAData[0].use_io_space = true;
                                LogDebug(pctx, 1, "I/O Read (%d-bit) transaction started", sbhe ? 16 : 8);
                            }
                            else if (iow)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_IO_WRITE_WORD : ISA_TRANS_IO_WRITE_BYTE;
                                ISAData[0].use_io_space = true;
                                LogDebug(pctx, 1, "I/O Write (%d-bit) transaction started", sbhe ? 16 : 8);
                            }
                            else if (memr)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_MEM_READ_WORD : ISA_TRANS_MEM_READ_BYTE;
                                ISAData[0].use_io_space = false;
                                LogDebug(pctx, 1, "Memory Read (%d-bit) transaction started", sbhe ? 16 : 8);
                            }
                            else if (memw)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_MEM_WRITE_WORD : ISA_TRANS_MEM_WRITE_BYTE;
                                ISAData[0].use_io_space = false;
                                LogDebug(pctx, 1, "Memory Write (%d-bit) transaction started", sbhe ? 16 : 8);
                            }
                            else
                            {
                                // No command line active - could be address latch only
                                ISAData[0].transaction_type = ISA_TRANS_NONE;
                                LogDebug(pctx, 1, "Address latch or unknown transaction");
                            }
                        }
                    }
                    break;
                    
                case ISA_STATE_T1:
                    // T1 state - Address phase
                    if (ale_falling_edge)
                    {
                        // Address latch complete on falling edge of ALE
                        ISABusData[0].addr_latch_state = 2;
                        ISABusData[0].latched_addr = ISABusData[0].partial_addr;
                        ISABusData[0].addr_valid = true;
                        ISAData[0].address = ISABusData[0].latched_addr & ((1 << GetAddressWidthBits()) - 1);
                        LogDebug(pctx, 2, "Address latched: 0x%08X", ISAData[0].address);
                    }
                    
                    // Check for command signals (normally asserted in T1 state)
                    if (ior_falling_edge || iow_falling_edge || memr_falling_edge || memw_falling_edge)
                    {
                        // Update transaction type if it wasn't determined yet
                        if (ISAData[0].transaction_type == ISA_TRANS_NONE)
                        {
                            if (ior_falling_edge)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_IO_READ_WORD : ISA_TRANS_IO_READ_BYTE;
                                ISAData[0].use_io_space = true;
                            }
                            else if (iow_falling_edge)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_IO_WRITE_WORD : ISA_TRANS_IO_WRITE_BYTE;
                                ISAData[0].use_io_space = true;
                            }
                            else if (memr_falling_edge)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_MEM_READ_WORD : ISA_TRANS_MEM_READ_BYTE;
                                ISAData[0].use_io_space = false;
                            }
                            else if (memw_falling_edge)
                            {
                                ISAData[0].transaction_type = sbhe ? ISA_TRANS_MEM_WRITE_WORD : ISA_TRANS_MEM_WRITE_BYTE;
                                ISAData[0].use_io_space = false;
                            }
                            
                            LogDebug(pctx, 2, "Transaction type updated to %s", 
                                     transaction_names[ISAData[0].transaction_type]);
                        }
                    }
                    
                    // Move to T2 state on the next BCLK rising edge
                    if (bclk_rising_edge)
                    {
                        ISAData[0].state = ISA_STATE_T2;
                        ISAData[0].bus_timing_cycles++;
                        LogDebug(pctx, 2, "Advancing to T2 state");
                    }
                    break;
                    
                case ISA_STATE_T2:
                    // T2 state - Data phase
                    if (bclk_rising_edge)
                    {
                        ISAData[0].bus_timing_cycles++;
                        
                        // Check for IOCHRDY (wait state insertion)
                        if (!iochrdy)
                        {
                            ISAData[0].state = ISA_STATE_TW;
                            LogDebug(pctx, 2, "Wait state inserted (IOCHRDY inactive)");
                        }
                        else
                        {
                            // Capture data on rising edge of T2 for write operations
                            if (ISAData[0].transaction_type == ISA_TRANS_IO_WRITE_BYTE ||
                                ISAData[0].transaction_type == ISA_TRANS_IO_WRITE_WORD ||
                                ISAData[0].transaction_type == ISA_TRANS_MEM_WRITE_BYTE ||
                                ISAData[0].transaction_type == ISA_TRANS_MEM_WRITE_WORD)
                            {
                                // For write operations, data should be valid during T2
                                ISABusData[0].data = data;
                                ISABusData[0].data_valid = true;
                                ISAData[0].data = data;
                                LogDebug(pctx, 2, "Data captured for write operation: 0x%04X", data);
                            }
                            
                            // Move to T3 state
                            ISAData[0].state = ISA_STATE_T3;
                            LogDebug(pctx, 2, "Advancing to T3 state");
                        }
                    }
                    
                    // For read operations, check data availability
                    if ((ISAData[0].transaction_type == ISA_TRANS_IO_READ_BYTE ||
                         ISAData[0].transaction_type == ISA_TRANS_IO_READ_WORD ||
                         ISAData[0].transaction_type == ISA_TRANS_MEM_READ_BYTE ||
                         ISAData[0].transaction_type == ISA_TRANS_MEM_READ_WORD) && 
                        data != prev_data)
                    {
                        // Data changed, might be target providing data
                        ISABusData[0].pending_data = data;
                        LogDebug(pctx, 5, "Potential data seen: 0x%04X", data);
                    }
                    break;
                    
                case ISA_STATE_TW:
                    // TW state - Wait states
                    if (bclk_rising_edge)
                    {
                        ISAData[0].bus_timing_cycles++;
                        ISAData[0].wait_states++;
                        
                        // Check if wait state is released
                        if (iochrdy)
                        {
                            ISAData[0].state = ISA_STATE_T3;
                            LogDebug(pctx, 2, "Wait state released, advancing to T3 (total wait states: %d)", 
                                     ISAData[0].wait_states);
                        }
                        else
                        {
                            LogDebug(pctx, 5, "Still in wait state (%d)", ISAData[0].wait_states);
                        }
                    }
                    
                    // Check for timeout condition
                    if (ISAData[0].wait_states > 20 && set_error_detection)
                    {
                        ISAData[0].timed_out = true;
                        ISAData[0].protocol_error = true;
                        snprintf(ISAData[0].error_message, sizeof(ISAData[0].error_message), 
                                 "Excessive wait states (%d) - possible timeout", ISAData[0].wait_states);
                        LogDebug(pctx, 0, "ERROR: %s", ISAData[0].error_message);
                        
                        // Force to T3 to complete transaction
                        ISAData[0].state = ISA_STATE_T3;
                    }
                    break;
                    
                case ISA_STATE_T3:
                    // T3 state - Completion phase
                    if (bclk_rising_edge)
                    {
                        ISAData[0].bus_timing_cycles++;
                        
                        // For read operations, data should be valid during T3
                        if ((ISAData[0].transaction_type == ISA_TRANS_IO_READ_BYTE ||
                             ISAData[0].transaction_type == ISA_TRANS_IO_READ_WORD ||
                             ISAData[0].transaction_type == ISA_TRANS_MEM_READ_BYTE ||
                             ISAData[0].transaction_type == ISA_TRANS_MEM_READ_WORD) && 
                            !ISABusData[0].data_valid)
                        {
                            ISABusData[0].data = data;
                            ISABusData[0].data_valid = true;
                            ISAData[0].data = data;
                            LogDebug(pctx, 2, "Data captured for read operation: 0x%04X", data);
                        }
                    }
                    
                    // Check for command signal deassertion to mark the end of transaction
                    if (ior_rising_edge || iow_rising_edge || memr_rising_edge || memw_rising_edge)
                    {
                        // Command signals deasserted, transaction is complete
                        ISAData[0].last_sequence = seq;
                        ISAData[0].end_time_ps = 0; // Would use timestamp here if available
                        
                        // Create sequence entry for the transaction
                        char addr_str[20];
                        FormatAddress(addr_str, sizeof(addr_str), ISAData[0].address);
                        
                        // Format for 8-bit vs 16-bit data
                        if (Is16BitTransaction(ISAData[0].transaction_type))
                        {
                            // 16-bit transaction
                            CreateSequenceEntry(
                                ISAData[0].sequence,
                                ISAData[0].transaction_type,
                                ISAData[0].protocol_error,
                                "%s | Addr: %s | Data: 0x%04X | Wait: %d",
                                transaction_names[ISAData[0].transaction_type],
                                addr_str,
                                ISABusData[0].data_valid ? ISAData[0].data : 0xFFFF,
                                ISAData[0].wait_states
                            );
                        }
                        else
                        {
                            // 8-bit transaction
                            CreateSequenceEntry(
                                ISAData[0].sequence,
                                ISAData[0].transaction_type,
                                ISAData[0].protocol_error,
                                "%s | Addr: %s | Data: 0x%02X | Wait: %d",
                                transaction_names[ISAData[0].transaction_type],
                                addr_str,
                                ISABusData[0].data_valid ? (ISAData[0].data & 0xFF) : 0xFF,
                                ISAData[0].wait_states
                            );
                        }
                        
                        // Reset for next transaction
                        ISAData[0].state = ISA_STATE_IDLE;
                        ISAData[0].transaction_type = ISA_TRANS_NONE;
                        LogDebug(pctx, 1, "Transaction completed");
                    }
                    
                    // Check for timeout (if commands stay asserted for too long)
                    if (ISAData[0].bus_timing_cycles > 10 && set_error_detection)
                    {
                        ISAData[0].timed_out = true;
                        ISAData[0].protocol_error = true;
                        snprintf(ISAData[0].error_message, sizeof(ISAData[0].error_message), 
                                 "Command signals remain asserted too long - possible timeout");
                        LogDebug(pctx, 0, "ERROR: %s", ISAData[0].error_message);
                        
                        // Create an error entry and reset state machine
                        char addr_str[20];
                        FormatAddress(addr_str, sizeof(addr_str), ISAData[0].address);
                        
                        CreateSequenceEntry(
                            ISAData[0].sequence,
                            ISA_TRANS_ERROR,
                            true,
                            "ERROR: %s transaction timed out | Addr: %s | Cycles: %d",
                            transaction_names[ISAData[0].transaction_type],
                            addr_str,
                            ISAData[0].bus_timing_cycles
                        );
                        
                        // Reset for next transaction
                        ISAData[0].state = ISA_STATE_IDLE;
                        ISAData[0].transaction_type = ISA_TRANS_NONE;
                    }
                    break;
                    
                case ISA_STATE_DMA_ACTIVE:
                    // DMA cycle processing
                    if (active_dma_channel != ISAData[0].active_dma_channel)
                    {
                        // DMA channel changed or deactivated
                        if (ISAData[0].active_dma_channel != -1)
                        {
                            // Complete previous DMA transaction
                            LogDebug(pctx, 1, "DMA channel %d cycle completed", ISAData[0].active_dma_channel);
                            
                            // Create sequence entry for DMA cycle
                            char addr_str[20];
                            FormatAddress(addr_str, sizeof(addr_str), ISAData[0].address);
                            
                            // Determine DMA transaction type
                            if (ISAData[0].transaction_type == ISA_TRANS_NONE)
                            {
                                if (ISAData[0].memr_active)
                                    ISAData[0].transaction_type = ISAData[0].is_16bit ? ISA_TRANS_DMA_READ_WORD : ISA_TRANS_DMA_READ_BYTE;
                                else if (ISAData[0].memw_active)
                                    ISAData[0].transaction_type = ISAData[0].is_16bit ? ISA_TRANS_DMA_WRITE_WORD : ISA_TRANS_DMA_WRITE_BYTE;
                                else if (ISAData[0].ior_active)
                                    ISAData[0].transaction_type = ISAData[0].is_16bit ? ISA_TRANS_DMA_READ_WORD : ISA_TRANS_DMA_READ_BYTE;
                                else if (ISAData[0].iow_active)
                                    ISAData[0].transaction_type = ISAData[0].is_16bit ? ISA_TRANS_DMA_WRITE_WORD : ISA_TRANS_DMA_WRITE_BYTE;
                            }
                            
                            CreateSequenceEntry(
                                ISAData[0].sequence,
                                ISAData[0].transaction_type,
                                ISAData[0].protocol_error,
                                "%s | Channel: %d | Addr: %s | Data: 0x%04X | TC: %s",
                                transaction_names[ISAData[0].transaction_type],
                                ISAData[0].active_dma_channel,
                                addr_str,
                                ISABusData[0].data_valid ? ISAData[0].data : 0xFFFF,
                                ISAData[0].tc_active ? "Yes" : "No"
                            );
                            
                            // Reset for next transaction
                            ISAData[0].state = ISA_STATE_IDLE;
                            ISAData[0].transaction_type = ISA_TRANS_NONE;
                            ISAData[0].active_dma_channel = -1;
                        }
                        
                        // Start new DMA transaction if applicable
                        if (active_dma_channel != -1)
                        {
                            ISAData[0].sequence = seq;
                            ISAData[0].active_dma_channel = active_dma_channel;
                            ISAData[0].tc_active = tc;
                            LogDebug(pctx, 1, "New DMA channel %d cycle started", active_dma_channel);
                        }
                    }
                    
                    // Process DMA read/write operations
                    if (ISAData[0].active_dma_channel != -1)
                    {
                        // Monitor command signals for DMA access type
                        if (ior && !ISAData[0].ior_active)
                        {
                            ISAData[0].ior_active = true;
                            LogDebug(pctx, 2, "DMA I/O Read active");
                        }
                        if (iow && !ISAData[0].iow_active)
                        {
                            ISAData[0].iow_active = true;
                            LogDebug(pctx, 2, "DMA I/O Write active");
                        }
                        if (memr && !ISAData[0].memr_active)
                        {
                            ISAData[0].memr_active = true;
                            LogDebug(pctx, 2, "DMA Memory Read active");
                        }
                        if (memw && !ISAData[0].memw_active)
                        {
                            ISAData[0].memw_active = true;
                            LogDebug(pctx, 2, "DMA Memory Write active");
                        }
                        
                        // Capture DMA address and data
                        if (!ISABusData[0].addr_valid && address != prev_address)
                        {
                            ISAData[0].address = address & ISA_ADDR_MASK & ((1 << GetAddressWidthBits()) - 1);
                            ISABusData[0].addr_valid = true;
                            LogDebug(pctx, 2, "DMA Address captured: 0x%08X", ISAData[0].address);
                        }
                        
                        if (!ISABusData[0].data_valid && data != prev_data)
                        {
                            ISAData[0].data = data;
                            ISABusData[0].data_valid = true;
                            LogDebug(pctx, 2, "DMA Data captured: 0x%04X", data);
                        }
                        
                        // Check for Terminal Count to end DMA transfer
                        if (tc && !ISAData[0].tc_active)
                        {
                            ISAData[0].tc_active = true;
                            LogDebug(pctx, 1, "DMA Terminal Count asserted");
                        }
                    }
                    break;
                    
                case ISA_STATE_REFRESH:
                    // Memory refresh cycle
                    if (bclk_rising_edge)
                    {
                        ISAData[0].bus_timing_cycles++;
                    }
                    
                    // Check for refresh signal deassertion to mark the end
                    if (refresh_rising_edge)
                    {
                        ISAData[0].last_sequence = seq;
                        
                        // Create sequence entry for refresh cycle
                        CreateSequenceEntry(
                            ISAData[0].sequence,
                            ISA_TRANS_REFRESH,
                            false,
                            "Memory Refresh Cycle | Cycles: %d",
                            ISAData[0].bus_timing_cycles
                        );
                        
                        // Reset for next transaction
                        ISAData[0].state = ISA_STATE_IDLE;
                        ISAData[0].transaction_type = ISA_TRANS_NONE;
                        LogDebug(pctx, 1, "Refresh cycle completed");
                    }
                    
                    // Check for timeout
                    if (ISAData[0].bus_timing_cycles > 10 && set_error_detection)
                    {
                        ISAData[0].timed_out = true;
                        ISAData[0].protocol_error = true;
                        snprintf(ISAData[0].error_message, sizeof(ISAData[0].error_message), 
                                 "Refresh cycle too long - possible timeout");
                        LogDebug(pctx, 0, "ERROR: %s", ISAData[0].error_message);
                        
                        // Create an error entry and reset state machine
                        CreateSequenceEntry(
                            ISAData[0].sequence,
                            ISA_TRANS_ERROR,
                            true,
                            "ERROR: Refresh cycle timed out | Cycles: %d",
                            ISAData[0].bus_timing_cycles
                        );
                        
                        // Reset for next transaction
                        ISAData[0].state = ISA_STATE_IDLE;
                        ISAData[0].transaction_type = ISA_TRANS_NONE;
                    }
                    break;
            }
            
            // Check for interrupt activity (can happen in any state)
            if (active_irq_line != ISAData[0].active_irq_line && set_irq_support)
            {
                if (active_irq_line != -1 && ISAData[0].active_irq_line == -1)
                {
                    // New interrupt detected
                    LogDebug(pctx, 1, "Interrupt line %d activated", active_irq_line);
                    
                    // Create sequence entry for interrupt
                    CreateSequenceEntry(
                        seq,
                        ISA_TRANS_NONE,
                        false,
                        "Interrupt Request | IRQ Line: %d",
                        active_irq_line
                    );
                }
                else if (active_irq_line == -1 && ISAData[0].active_irq_line != -1)
                {
                    // Interrupt cleared
                    LogDebug(pctx, 1, "Interrupt line %d cleared", ISAData[0].active_irq_line);
                }
                
                ISAData[0].active_irq_line = active_irq_line;
            }
            
            // Check for IOCHK errors
            if (iochk && !IsActiveLow(ISA_IOCHK, prev_ctrl_signals) && set_error_detection)
            {
                LogDebug(pctx, 0, "I/O Channel Check Error (IOCHK#) detected");
                
                // Create sequence entry for IOCHK error
                CreateSequenceEntry(
                    seq,
                    ISA_TRANS_ERROR,
                    true,
                    "ERROR: I/O Channel Check (IOCHK#) detected"
                );
            }
            
            // Save previous signal states for edge detection in next iteration
            prev_ctrl_signals = ctrl_signals;
            prev_dma_signals = dma_signals;
            prev_irq_signals = irq_signals;
            prev_address = address;
            prev_data = data;
        }
        
        // Final check for any incomplete transactions
        if (ISAData[0].state != ISA_STATE_IDLE && ISAData[0].transaction_type != ISA_TRANS_NONE)
        {
            LogDebug(pctx, 0, "Warning: Incomplete transaction at end of capture");
            
            // Create a warning entry for the incomplete transaction
            char addr_str[20] = "Unknown";
            if (ISABusData[0].addr_valid)
            {
                FormatAddress(addr_str, sizeof(addr_str), ISAData[0].address);
            }
            
            CreateSequenceEntry(
                ISAData[0].sequence,
                ISAData[0].transaction_type,
                true,
                "WARNING: Incomplete %s | Addr: %s | State: %d",
                transaction_names[ISAData[0].transaction_type],
                addr_str,
                ISAData[0].state
            );
        }
        
        // Sort out the text pointers
        it = SeqDataVector.begin();
        while (it != SeqDataVector.end())
        {
            it->seq_data.textp = it->seq_data.text;
            it->seq_data.text2 = it->seq_data.text2_buf;
            it++;
        }
        
        LogDebug(pctx, 0, "Processing completed - found %d sequences", SeqDataVector.size());
    }
    
    // Find the requested sequence
    it = SeqDataVector.begin();
    while ((it != SeqDataVector.end()) && (seqinfo == NULL))
    {
        if (it->seq_number == initseq)
        {
            seqinfo = &(it->seq_data);
            LogDebug(pctx, 9, "%s: seq: %d text: [%s]", "*ParseSeq", initseq, seqinfo->textp);
        }
        it++;
    }
    
    return seqinfo;
}

int ParseMarkNext(struct pctx *pctx, int seq, int a3)
{
    LogDebug(pctx, 9, "%s: sequence %d, a3 %d", "ParseMarkNext", seq, a3);
    
    // Find the next marked sequence
    if (SeqDataVector.size() == 0)
        return seq;
    
    TVSeqData::iterator it = SeqDataVector.begin();
    while (it != SeqDataVector.end())
    {
        if (it->seq_number > seq)
            return it->seq_number;
        it++;
    }
    
    // If we reach here, no next sequence found, return the current one
    return seq;
}

int ParseMarkSet(struct pctx *pctx, int seq, int a3)
{
    LogDebug(pctx, 9, "%s", "ParseMarkSet");
    return 0;
}

int ParseMarkGet(struct pctx *pctx, int seq)
{
    LogDebug(pctx, 9, "%s: sequence %d", "ParseMarkGet", seq);
    return seq;
}

int ParseMarkMenu(struct pctx *pctx, int seq, int a3, int a4, int a5)
{
    LogDebug(pctx, 0, "%s: sequence %d, a3: %08x, a4: %08x, a5: %08x", "ParseMarkMenu", seq, a3, a4, a5);
    return 0;
}

int ParseInfo(struct pctx *pctx, unsigned int request)
{
    LogDebug(pctx, 6, "%s: %s", "ParseInfo",
        request > ARRAY_SIZE(modeinfo_names) ? "invalid" : modeinfo_names[request]);
    
    switch(request) {
        case MODEINFO_MAX_BUS:
            return ARRAY_SIZE(businfo);
        case MODEINFO_MAX_GROUP:
            return ARRAY_SIZE(groupinfo);
        case MODEINFO_GETNAME:
            return (int)"ISA";
        case 3:
            return 1;
        case MODEINFO_MAX_MODE:
            return ARRAY_SIZE(modeinfo);
        default:
            LogDebug(pctx, 9, "%s: invalid request: %d", "ParseInfo", request);
            return 0;
    }
    
    return 0;
}

struct businfo *ParseBusInfo(struct pctx *pctx, uint16_t bus)
{
    LogDebug(pctx, 6, "%s: %08x", "ParseBusInfo", bus);
    // Reset for parsing data
    SeqDataVector.clear();
    processing_done = 0;
    
    if (bus >= ARRAY_SIZE(businfo))
        return NULL;
    
    return (struct businfo*) businfo + bus;
}

struct groupinfo *ParseGroupInfo(struct pctx *pctx, uint16_t group)
{
    LogDebug(pctx, 9, "%s: %08x", "ParseGroupInfo", group);
    
    if (group > ARRAY_SIZE(groupinfo))
        return NULL;
    
    return (struct groupinfo*) groupinfo + group;
}

struct modeinfo *ParseModeInfo(struct pctx *pctx, uint16_t mode)
{
    LogDebug(pctx, 9, "%s: %d", "ParseModeInfo", mode);
    
    if (mode > ARRAY_SIZE(modeinfo))
        return NULL;
    
    return (struct modeinfo*) modeinfo + mode;
}

int ParseModeGetPut(struct pctx *pctx, int mode, int value, int request)
{
    LogDebug(pctx, 9, "%s: mode: %d value: %d request: %d ", "ParseModeGetPut", mode, value, request);
    
    // mode is an index to the setting
    // value is the selected index
    // request: 0=read, 1=write, 2=write/read (in that order!)
    
    // Write values?
    if ((request == 1) || (request == 2))
    {
        // Set values
        switch (mode)
        {
            case 0:
                // ADDR_WIDTH setting
                set_addr_width = value;
                break;
            case 1:
                // BUS_SPEED setting
                set_bus_speed = value;
                break;
            case 2:
                // DMA_SUPPORT setting
                set_dma_support = value;
                break;
            case 3:
                // REFRESH_SUPPORT setting
                set_refresh_support = value;
                break;
            case 4:
                // IRQ_SUPPORT setting
                set_irq_support = value;
                break;
            case 5:
                // TIMING_MODE setting
                set_timing_mode = value;
                break;
            case 6:
                // ERROR_DETECTION setting
                set_error_detection = value;
                break;
            default:
                break;
        }
    }
    
    // Read values?
    if ((request == 0) || (request == 2))
    {
        // Read values
        value = 0;  // Choose default value
        switch (mode)
        {
            case 0:
                // ADDR_WIDTH setting
                value = set_addr_width;
                break;
            case 1:
                // BUS_SPEED setting
                value = set_bus_speed;
                break;
            case 2:
                // DMA_SUPPORT setting
                value = set_dma_support;
                break;
            case 3:
                // REFRESH_SUPPORT setting
                value = set_refresh_support;
                break;
            case 4:
                // IRQ_SUPPORT setting
                value = set_irq_support;
                break;
            case 5:
                // TIMING_MODE setting
                value = set_timing_mode;
                break;
            case 6:
                // ERROR_DETECTION setting
                value = set_error_detection;
                break;
            default:
                value = 0;
                break;
        }
    }
    
    LogDebug(pctx, 9, "%s: addr_width: %d, bus_speed: %d, dma: %d, refresh: %d, irq: %d, timing: %d, error: %d", 
        "ParseModeGetPut", set_addr_width, set_bus_speed, set_dma_support, 
        set_refresh_support, set_irq_support, set_timing_mode, set_error_detection);
    return value;
}

int ParseDisasmReinit(struct pctx *pctx, int request)
{
    // Initialize parsing
    LogDebug(pctx, 9, "%s: pctx %08x, request=%d", "ParseDisasmReinit", pctx, request);
    return 1;
}