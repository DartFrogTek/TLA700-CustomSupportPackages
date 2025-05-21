#include <vector>
using namespace std;
#include "ISA_minimal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// Enable debugging if needed
//#define WITH_DEBUG

#ifdef WITH_DEBUG
static void LogDebug(struct pctx *pctx, int level, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OutputDebugString(buf);
}
#endif

/*********************************************************
        Globals signal setup
*********************************************************/
const char *modeinfo_names[MODEINFO_MAX] = {
    "MAX_BUS",
    "MAX_GROUP",
    "MAX_MODE",
    "3",
    "GETNAME"
};

// Define groups - simplified version with only 3 groups
const struct groupinfo groupinfo[] = { 
    { "ISA_Control", 0, 0, 0, 0, 0x000007FF, 0 },  // ISA control signals (11 signals)
    { "ISA_Addr", 0, 0, 0, 0, 0x0000FFFF, 0 },     // Address lines (16 bits)
    { "ISA_Data", 0, 0, 0, 0, 0x000000FF, 0 },     // Data lines (8 bits)
    { "Traffic", 0, 0, 2, 1, 0x80, sizeof("Traffic")-1 } 
};

const struct businfo businfo[] = { { 0, 0, 0, 0, 0, 0x20000, NULL, 0 } };

// Define settings
const char *addr_width[] = { "16-bit", "20-bit", "24-bit", NULL };
const char *timing_mode[] = { "Fast Mode", "Normal Mode", "Slow Mode", NULL };
const char *data_width[] = { "8-bit", "16-bit", NULL };

const struct modeinfo modeinfo[] = { 
    { "ADDR_WIDTH", addr_width, 0, 2 },
    { "TIMING_MODE", timing_mode, 1, 2 },
    { "DATA_WIDTH", data_width, 0, 1 }
};

// Names for the transaction types
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
    "Memory Refresh",
    "Error"
};

/*********************************************************
        ISA analysis data
*********************************************************/
static TISAData ISAData[1];       // Active transaction data
static TISABusData ISABusData[1]; // Bus state tracking
static TISAFeatureConfig FeatureConfig; // Feature configuration
static int processing_done;
TVSeqData SeqDataVector;          // Vector with analysis results

/*********************************************************
        Helpers
*********************************************************/

// Helper function to get address width in bits based on setting
static int GetAddressWidthBits()
{
    switch (FeatureConfig.addr_width)
    {
        case 0: return 16;
        case 1: return 20;
        case 2: return 24;
        default: return 16;
    }
}

// Helper function to format address based on address width
static void FormatAddress(char* buf, size_t buf_size, uint32_t addr)
{
    switch (FeatureConfig.addr_width)
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
    return (value & signal) == 0 ? 1 : 0;
}

// Helper to check if the given value is active high (normally low)
static int IsActiveHigh(uint32_t signal, uint32_t value)
{
    return (value & signal) != 0 ? 1 : 0;
}

// Helper function to create a new sequence data entry
static void CreateSequenceEntry(int seq_number, int trans_type, int error_flag, const char* format, ...)
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
            return 1;
        default:
            return 0;
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
    FeatureConfig.enabled_features = 0;
    FeatureConfig.addr_width = 0;        // 16-bit
    FeatureConfig.data_width = 0;        // 8-bit
    FeatureConfig.addr_group = 1;        // Group 1 for address
    FeatureConfig.data_group = 2;        // Group 2 for data
    FeatureConfig.control_group = 0;     // Group 0 for control
    
    SeqDataVector.clear();
    processing_done = 0;
    
    LogDebug(ret, 0, "ISA Protocol Analyzer Minimal Version Initialization Completed");
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
    pctx->func.rda_free(pctx);
    return 0;
}

struct sequence *ParseSeq(struct pctx *pctx, int initseq)
{
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
        
        // Previous signal states for edge detection
        uint32_t prev_ctrl_signals = 0;
        uint32_t prev_address = 0;
        uint32_t prev_data = 0;
        
        int bclk_cycles = 0; // Counter for BCLK cycles
        
        // Now loop through all the samples
        for (int seq = firstseq; seq <= lastseq; seq++)
        {
            // Get signal values for each group
            uint32_t ctrl_signals = pctx->func.LAGroupValue(pctx->lactx, seq, FeatureConfig.control_group);
            uint32_t address = pctx->func.LAGroupValue(pctx->lactx, seq, FeatureConfig.addr_group);
            uint16_t data = pctx->func.LAGroupValue(pctx->lactx, seq, FeatureConfig.data_group) & 
                           (FeatureConfig.data_width == 0 ? 0xFF : 0xFFFF);
            
            LogDebug(pctx, 5, "Seq: %d, Ctrl: 0x%08X, Addr: 0x%08X, Data: 0x%04X", 
                     seq, ctrl_signals, address, data);
            
            // Decode control signals
            int bclk = IsActiveHigh(ISA_BCLK, ctrl_signals);
            int ale = IsActiveHigh(ISA_ALE, ctrl_signals);
            int ior = IsActiveLow(ISA_IOR, ctrl_signals);
            int iow = IsActiveLow(ISA_IOW, ctrl_signals);
            int memr = IsActiveLow(ISA_MEMR, ctrl_signals);
            int memw = IsActiveLow(ISA_MEMW, ctrl_signals);
            int refresh = (ctrl_signals & ISA_REFRESH) ? IsActiveLow(ISA_REFRESH, ctrl_signals) : 0;
            int sbhe = (ctrl_signals & ISA_SBHE) ? IsActiveLow(ISA_SBHE, ctrl_signals) : 0;
            int iochrdy = (ctrl_signals & ISA_IOCHRDY) ? IsActiveHigh(ISA_IOCHRDY, ctrl_signals) : 1;
            int aen = (ctrl_signals & ISA_AEN) ? IsActiveHigh(ISA_AEN, ctrl_signals) : 0;
            int reset = (ctrl_signals & ISA_RESET) ? IsActiveHigh(ISA_RESET, ctrl_signals) : 0;
            
            // Edge detection
            int bclk_rising_edge = bclk && !IsActiveHigh(ISA_BCLK, prev_ctrl_signals);
            int bclk_falling_edge = !bclk && IsActiveHigh(ISA_BCLK, prev_ctrl_signals);
            int ale_rising_edge = ale && !IsActiveHigh(ISA_ALE, prev_ctrl_signals);
            int ale_falling_edge = !ale && IsActiveHigh(ISA_ALE, prev_ctrl_signals);
            
            int ior_falling_edge = ior && !IsActiveLow(ISA_IOR, prev_ctrl_signals);
            int iow_falling_edge = iow && !IsActiveLow(ISA_IOW, prev_ctrl_signals);
            int memr_falling_edge = memr && !IsActiveLow(ISA_MEMR, prev_ctrl_signals);
            int memw_falling_edge = memw && !IsActiveLow(ISA_MEMW, prev_ctrl_signals);
            int refresh_falling_edge = refresh && !(prev_ctrl_signals & ISA_REFRESH ? 
                                      IsActiveLow(ISA_REFRESH, prev_ctrl_signals) : 0);
            
            int ior_rising_edge = !ior && IsActiveLow(ISA_IOR, prev_ctrl_signals);
            int iow_rising_edge = !iow && IsActiveLow(ISA_IOW, prev_ctrl_signals);
            int memr_rising_edge = !memr && IsActiveLow(ISA_MEMR, prev_ctrl_signals);
            int memw_rising_edge = !memw && IsActiveLow(ISA_MEMW, prev_ctrl_signals);
            int refresh_rising_edge = !refresh && (prev_ctrl_signals & ISA_REFRESH ? 
                                     IsActiveLow(ISA_REFRESH, prev_ctrl_signals) : 0);
            
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
                        CreateSequenceEntry(seq, ISA_TRANS_NONE, 0, "SYSTEM RESET");
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
                        ISAData[0].is_16bit = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) ? sbhe : 0;
                        ISAData[0].timed_out = 0;
                        ISAData[0].protocol_error = 0;
                        
                        // Track active command signals
                        ISAData[0].ior_active = ior;
                        ISAData[0].iow_active = iow;
                        ISAData[0].memr_active = memr;
                        ISAData[0].memw_active = memw;
                        ISAData[0].refresh_active = refresh;
                        ISAData[0].sbhe_active = sbhe;
                        ISAData[0].iochrdy_active = iochrdy;
                        ISAData[0].aen_active = aen;
                        
                        // Capture address (first part from address lines)
                        ISABusData[0].addr_latch_state = 1;
                        ISABusData[0].partial_addr = address & 
                            ((1 << GetAddressWidthBits()) - 1);
                        ISABusData[0].addr_valid = 0;
                        ISABusData[0].data_valid = 0;
                        
                        // Check for refresh cycle
                        if (refresh && (ctrl_signals & ISA_REFRESH))
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
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_IO_READ_WORD : ISA_TRANS_IO_READ_BYTE;
                                ISAData[0].use_io_space = 1;
                                LogDebug(pctx, 1, "I/O Read (%d-bit) transaction started", 
                                         (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 16 : 8);
                            }
                            else if (iow)
                            {
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_IO_WRITE_WORD : ISA_TRANS_IO_WRITE_BYTE;
                                ISAData[0].use_io_space = 1;
                                LogDebug(pctx, 1, "I/O Write (%d-bit) transaction started", 
                                         (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 16 : 8);
                            }
                            else if (memr)
                            {
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_MEM_READ_WORD : ISA_TRANS_MEM_READ_BYTE;
                                ISAData[0].use_io_space = 0;
                                LogDebug(pctx, 1, "Memory Read (%d-bit) transaction started", 
                                         (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 16 : 8);
                            }
                            else if (memw)
                            {
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_MEM_WRITE_WORD : ISA_TRANS_MEM_WRITE_BYTE;
                                ISAData[0].use_io_space = 0;
                                LogDebug(pctx, 1, "Memory Write (%d-bit) transaction started", 
                                         (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 16 : 8);
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
                        ISABusData[0].addr_valid = 1;
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
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_IO_READ_WORD : ISA_TRANS_IO_READ_BYTE;
                                ISAData[0].use_io_space = 1;
                            }
                            else if (iow_falling_edge)
                            {
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_IO_WRITE_WORD : ISA_TRANS_IO_WRITE_BYTE;
                                ISAData[0].use_io_space = 1;
                            }
                            else if (memr_falling_edge)
                            {
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_MEM_READ_WORD : ISA_TRANS_MEM_READ_BYTE;
                                ISAData[0].use_io_space = 0;
                            }
                            else if (memw_falling_edge)
                            {
                                ISAData[0].transaction_type = (FeatureConfig.enabled_features & ISA_FEATURE_16BIT) && sbhe ? 
                                                          ISA_TRANS_MEM_WRITE_WORD : ISA_TRANS_MEM_WRITE_BYTE;
                                ISAData[0].use_io_space = 0;
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
                        if ((ctrl_signals & ISA_IOCHRDY) && !iochrdy)
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
                                ISABusData[0].data_valid = 1;
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
                        if (!(ctrl_signals & ISA_IOCHRDY) || iochrdy)
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
                    if (ISAData[0].wait_states > 20)
                    {
                        ISAData[0].timed_out = 1;
                        ISAData[0].protocol_error = 1;
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
                            ISABusData[0].data_valid = 1;
                            ISAData[0].data = data;
                            LogDebug(pctx, 2, "Data captured for read operation: 0x%04X", data);
                        }
                    }
                    
                    // Check for command signal deassertion to mark the end of transaction
                    if (ior_rising_edge || iow_rising_edge || memr_rising_edge || memw_rising_edge)
                    {
                        // Command signals deasserted, transaction is complete
                        ISAData[0].last_sequence = seq;
                        
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
                    if (ISAData[0].bus_timing_cycles > 10)
                    {
                        ISAData[0].timed_out = 1;
                        ISAData[0].protocol_error = 1;
                        snprintf(ISAData[0].error_message, sizeof(ISAData[0].error_message), 
                                 "Command signals remain asserted too long - possible timeout");
                        LogDebug(pctx, 0, "ERROR: %s", ISAData[0].error_message);
                        
                        // Create an error entry and reset state machine
                        char addr_str[20];
                        FormatAddress(addr_str, sizeof(addr_str), ISAData[0].address);
                        
                        CreateSequenceEntry(
                            ISAData[0].sequence,
                            ISA_TRANS_ERROR,
                            1,
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
                            0,
                            "Memory Refresh Cycle | Cycles: %d",
                            ISAData[0].bus_timing_cycles
                        );
                        
                        // Reset for next transaction
                        ISAData[0].state = ISA_STATE_IDLE;
                        ISAData[0].transaction_type = ISA_TRANS_NONE;
                        LogDebug(pctx, 1, "Refresh cycle completed");
                    }
                    
                    // Check for timeout
                    if (ISAData[0].bus_timing_cycles > 10)
                    {
                        ISAData[0].timed_out = 1;
                        ISAData[0].protocol_error = 1;
                        snprintf(ISAData[0].error_message, sizeof(ISAData[0].error_message), 
                                 "Refresh cycle too long - possible timeout");
                        LogDebug(pctx, 0, "ERROR: %s", ISAData[0].error_message);
                        
                        // Create an error entry and reset state machine
                        CreateSequenceEntry(
                            ISAData[0].sequence,
                            ISA_TRANS_ERROR,
                            1,
                            "ERROR: Refresh cycle timed out | Cycles: %d",
                            ISAData[0].bus_timing_cycles
                        );
                        
                        // Reset for next transaction
                        ISAData[0].state = ISA_STATE_IDLE;
                        ISAData[0].transaction_type = ISA_TRANS_NONE;
                    }
                    break;
            }
            
            // Save previous signal states for edge detection in next iteration
            prev_ctrl_signals = ctrl_signals;
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
                1,
                "WARNING: Incomplete %s | Addr: %s | State: %d",
                transaction_names[ISAData[0].transaction_type],
                addr_str,
                ISAData[0].state
            );
        }
        
        // Set up the text pointers
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
            LogDebug(pctx, 9, "%s: seq: %d text: [%s]", "ParseSeq", initseq, seqinfo->textp);
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
            return (int)"ISA_Min";
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
    
    // Write values?
    if ((request == 1) || (request == 2))
    {
        // Set values
        switch (mode)
        {
            case 0:
                // ADDR_WIDTH setting
                FeatureConfig.addr_width = value;
                
                // Update feature flags
                if (value == 0) 
                    FeatureConfig.enabled_features &= ~(ISA_FEATURE_20BIT_ADDR | ISA_FEATURE_24BIT_ADDR);
                else if (value == 1) {
                    FeatureConfig.enabled_features |= ISA_FEATURE_20BIT_ADDR;
                    FeatureConfig.enabled_features &= ~ISA_FEATURE_24BIT_ADDR;
                }
                else if (value == 2) {
                    FeatureConfig.enabled_features |= ISA_FEATURE_24BIT_ADDR;
                }
                break;
                
            case 1:
                // TIMING_MODE setting - no feature flags
                // Just store the value
                break;
                
            case 2:
                // DATA_WIDTH setting
                FeatureConfig.data_width = value;
                
                // Update feature flags
                if (value == 0)
                    FeatureConfig.enabled_features &= ~ISA_FEATURE_16BIT;
                else
                    FeatureConfig.enabled_features |= ISA_FEATURE_16BIT;
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
                value = FeatureConfig.addr_width;
                break;
                
            case 1:
                // TIMING_MODE setting - no feature flags
                // Just return the default (1 - Normal Mode)
                value = 1;
                break;
                
            case 2:
                // DATA_WIDTH setting
                value = FeatureConfig.data_width;
                break;
                
            default:
                value = 0;
                break;
        }
    }
    
    LogDebug(pctx, 9, "%s: addr_width: %d, data_width: %d, features: 0x%X", 
        "ParseModeGetPut", FeatureConfig.addr_width, FeatureConfig.data_width, 
        FeatureConfig.enabled_features);
        
    return value;
}

int ParseDisasmReinit(struct pctx *pctx, int request)
{
    // Initialize parsing
    LogDebug(pctx, 9, "%s: pctx %p, request=%d", "ParseDisasmReinit", pctx, request);
    return 1;
}