#include <vector>
using namespace std;
#include "PCI.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <windows.h>
#include <time.h>
#include <errno.h>
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

// Define groups
const struct groupinfo groupinfo[] = { 
    { "PCI", 0, 0, 0, 0, 0xFFFFFF, 0 },        // All PCI signals
    { "PCISig", 0, 0, 0, 0, 0xFFFF, 0 },       // PCI control signals
    { "PCIAD", 0, 0, 0, 0, 0xFF0000, 0 },      // PCI address/data signals
    { "PCIInt", 0, 0, 0, 0, 0x3C00, 0 },       // PCI interrupt signals
    { "Traffic", 0, 0, 2, 1, 0x80, sizeof("Traffic")-1 } 
};

const struct businfo businfo[] = { { 0, 0, 0, 0, 0, 0x20000, NULL, 0 } };

// Define settings
const char *pci_bus_width[] = { "32-bit", "64-bit", NULL };
const char *pci_bus_speed[] = { "33 MHz", "66 MHz", NULL };
const char *pci_arb_mode[] = { "Simple", "Fairness", "Priority", NULL };
const char *pci_cache_line_size[] = { "Disabled", "16 Bytes", "32 Bytes", "64 Bytes", NULL };
const char *pci_latency[] = { "Minimal", "Standard", "Extended", NULL };
const char *pci_retry_policy[] = { "Immediate Retry", "Delayed Retry", NULL };

const struct modeinfo modeinfo[] = { 
    { "BUS_WIDTH", pci_bus_width, 0, 1 },
    { "BUS_SPEED", pci_bus_speed, 0, 1 },
    { "ARB_MODE", pci_arb_mode, 0, 2 },
    { "CACHELINE", pci_cache_line_size, 0, 3 },
    { "LATENCY", pci_latency, 0, 2 },
    { "RETRY_POLICY", pci_retry_policy, 0, 1 }
};

// PCI Command names for better readability
const char* pci_cmd_names[] = {
    "Interrupt Acknowledge",  // 0x0
    "Special Cycle",          // 0x1
    "I/O Read",               // 0x2
    "I/O Write",              // 0x3
    "Reserved (0x4)",         // 0x4
    "Reserved (0x5)",         // 0x5
    "Memory Read",            // 0x6
    "Memory Write",           // 0x7
    "Reserved (0x8)",         // 0x8
    "Reserved (0x9)",         // 0x9
    "Configuration Read",     // 0xA
    "Configuration Write",    // 0xB
    "Memory Read Multiple",   // 0xC
    "Dual Address Cycle",     // 0xD
    "Memory Read Line",       // 0xE
    "Memory Write and Invalidate" // 0xF
};

// PCI Completion names
const char* pci_comp_names[] = {
    "Normal",            // 0
    "Master Abort",      // 1
    "Target Abort",      // 2
    "Retry",             // 3
    "Disconnect"         // 4
};

// PCI Burst types
const char* pci_burst_names[] = {
    "Single Transfer",   // 0
    "Multiple Transfer", // 1
    "Line Transfer",     // 2
    "Continuous"         // 3
};

/*********************************************************
        PCI analysis data
*********************************************************/
static TPCIData PCIData;     // Current transaction being processed
static vector<TPCIData> PCITransactions; // All completed transactions
static TVSeqData SeqDataVector;    // Vector with sequence results

// Settings
static int set_bus_width;          // 32-bit or 64-bit
static int set_bus_speed;          // 33MHz or 66MHz
static int set_arb_mode;           // Arbitration mode
static int set_cache_line_size;    // Cache line size
static int set_latency;            // Latency timer
static int set_retry_policy;       // Retry policy
static int processing_done;        // Flag to avoid reprocessing

// State machine variables
static uint32_t previous_signals = 0;  // Previous sample signals
static bool in_transaction = false;    // Currently processing a transaction?
static uint8_t current_command = 0;    // Current PCI command
static uint32_t current_state = PCI_IDLE; // Current state machine state

/*********************************************************
        Helper Functions
*********************************************************/
// Extract specific signal from value
static bool signal_active(uint32_t value, uint32_t signal_mask)
{
    return (value & signal_mask) != 0;
}

// Convert PCI command code to string
static const char* get_command_string(uint8_t command)
{
    if (command < 16) {
        return pci_cmd_names[command];
    } else {
        return "Unknown";
    }
}

// Extract C/BE# signals (Command/Byte Enable)
static uint8_t extract_command(uint32_t value)
{
    return (value & PCI_C_BE) >> 16;
}

// Extract byte enables
static uint8_t extract_byte_enables(uint32_t value)
{
    return (value & PCI_C_BE) >> 16;
}

// Extract address/data from AD signals
static uint32_t extract_ad(uint32_t value)
{
    return (value & PCI_AD) >> 22;
}

// Check if a signal transition occurred (rising edge)
static bool signal_rose(uint32_t current, uint32_t previous, uint32_t signal_mask)
{
    return ((current & signal_mask) != 0) && ((previous & signal_mask) == 0);
}

// Check if a signal transition occurred (falling edge)
static bool signal_fell(uint32_t current, uint32_t previous, uint32_t signal_mask)
{
    return ((current & signal_mask) == 0) && ((previous & signal_mask) != 0);
}

// Check if PCI reset occurred
static bool reset_active(uint32_t value)
{
    return (value & PCI_RST) == 0; // RST# is active low
}

// Check for parity error
static bool check_parity(uint32_t value)
{
    // Simplified parity check - real implementation would compute parity over AD and C/BE
    return (value & PCI_PERR) == 0; // PERR# is active low
}

// Check for system error
static bool system_error(uint32_t value)
{
    return (value & PCI_SERR) == 0; // SERR# is active low
}

// Determine if a master abort occurred
static bool is_master_abort(uint32_t value)
{
    // Master abort occurs when DEVSEL# is not asserted within 5 clocks after FRAME#
    // This is a simplified check
    return ((value & PCI_FRAME) == 0) && ((value & PCI_DEVSEL) != 0);
}

// Determine if a target abort occurred
static bool is_target_abort(uint32_t value)
{
    // Target abort occurs when DEVSEL# is deasserted during a transaction
    // This is a simplified check
    return ((value & PCI_TRDY) != 0) && ((value & PCI_STOP) == 0) && ((value & PCI_DEVSEL) != 0);
}

// Determine if this is a configuration transaction
static bool is_config_transaction(uint8_t command)
{
    return (command == PCI_CMD_CONFIG_READ || command == PCI_CMD_CONFIG_WRITE);
}

// Determine if this is a memory transaction
static bool is_memory_transaction(uint8_t command)
{
    return (command == PCI_CMD_MEM_READ || command == PCI_CMD_MEM_WRITE || 
            command == PCI_CMD_MEM_READ_MULTIPLE || command == PCI_CMD_MEM_READ_LINE || 
            command == PCI_CMD_MEM_WRITE_AND_INV);
}

// Determine if this is an I/O transaction
static bool is_io_transaction(uint8_t command)
{
    return (command == PCI_CMD_IO_READ || command == PCI_CMD_IO_WRITE);
}

// Determine if this is a burst transaction
static bool is_burst_transaction(uint8_t command)
{
    return (command == PCI_CMD_MEM_READ_MULTIPLE || command == PCI_CMD_MEM_READ_LINE || 
            command == PCI_CMD_MEM_WRITE_AND_INV);
}

// Format a transaction into a readable string
static void format_transaction(char* buffer, size_t buffer_size, const TPCIData& transaction)
{
    const char* cmd_str = get_command_string(transaction.command);
    char addr_str[32];
    char data_str[64] = {0};
    char detail_str[128] = {0};
    
    // Format the address
    if (transaction.is_64bit) {
        snprintf(addr_str, sizeof(addr_str), "0x%016llX", transaction.address);
    } else {
        snprintf(addr_str, sizeof(addr_str), "0x%08X", (uint32_t)transaction.address);
    }
    
    // Format data (first data phase only in summary)
    if (transaction.data_phase_count > 0) {
        snprintf(data_str, sizeof(data_str), "Data:0x%08X", transaction.data[0]);
    }
    
    // Add additional details based on transaction type
    if (is_config_transaction(transaction.command)) {
        snprintf(detail_str, sizeof(detail_str), "Dev:%d Func:%d %s", 
                 transaction.device_num, transaction.function_num,
                 transaction.is_type1_config ? "Type1" : "Type0");
    } else if (transaction.completion_type != PCI_COMP_NORMAL) {
        snprintf(detail_str, sizeof(detail_str), "Completion:%s", 
                 pci_comp_names[transaction.completion_type]);
    } else if (transaction.data_phase_count > 1) {
        snprintf(detail_str, sizeof(detail_str), "Burst:%s Phases:%d", 
                 pci_burst_names[transaction.burst_type], transaction.data_phase_count);
    }
    
    // Format byte enables if available
    char be_str[16] = {0};
    if (transaction.data_phase_count > 0) {
        snprintf(be_str, sizeof(be_str), "BE:0x%X", transaction.byte_enables[0]);
    }
    
    // Combine all components
    snprintf(buffer, buffer_size, "%s %s %s %s %s", 
             cmd_str, addr_str, data_str, be_str, detail_str);
}

/*********************************************************
        Debug Logging
*********************************************************/
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
        DLL functions
*********************************************************/
struct pctx *ParseReinit(struct pctx *pctx, struct lactx *lactx, struct lafunc *func)
{
    // Called upon DLL startup and refreshing the data
    struct pctx *ret;
    SeqDataVector.clear();
    PCITransactions.clear();
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
    
    // defaults
    set_bus_width = 0;           // 32-bit
    set_bus_speed = 0;           // 33 MHz
    set_arb_mode = 0;            // Simple
    set_cache_line_size = 0;     // Disabled
    set_latency = 1;             // Standard
    set_retry_policy = 0;        // Immediate
    
    // Initialize state machine
    in_transaction = false;
    current_state = PCI_IDLE;
    previous_signals = 0;
    
    // Reset current transaction data
    memset(&PCIData, 0, sizeof(PCIData));
    
    LogDebug(ret, 0, "PCI Protocol Analyzer Initialization finished");
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
    PCITransactions.clear();
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
        
        // Get sequence range
        int firstseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_FIRST_SEQUENCE, -1);
        int lastseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_LAST_SEQUENCE, -1);
        LogDebug(pctx, 0, "initseq: %d, firstseq: %d, last seq: %d", initseq, firstseq, lastseq);
        
        // Initialize state
        current_state = PCI_IDLE;
        in_transaction = false;
        memset(&PCIData, 0, sizeof(PCIData));
        
        // Clear previous data
        PCITransactions.clear();
        SeqDataVector.clear();
        
        // Now loop through all the samples to find PCI transactions
        for (int seq = firstseq; seq <= lastseq; seq++)
        {
            uint32_t signals = pctx->func.LAGroupValue(pctx->lactx, seq, 0);
            bool clock_edge = signal_rose(signals, previous_signals, PCI_CLK);
            
            LogDebug(pctx, 9, "Seq %d: Signals=0x%08X State=%d", seq, signals, current_state);
            
            // Only process on rising edge of clock or when reset is active
            if (clock_edge || reset_active(signals))
            {
                // Check for reset
                if (reset_active(signals))
                {
                    // Reset detected, abort any current transaction and return to idle
                    if (in_transaction)
                    {
                        // Create an entry for the aborted transaction
                        TSeqData SeqData;
                        memset(&SeqData, 0, sizeof(SeqData));
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), 
                                "PCI Reset during transaction");
                        SeqData.seq_data.flags = 4; // Red background for error
                        SeqData.seq_number = seq;
                        SeqDataVector.push_back(SeqData);
                        
                        // Reset state machine
                        in_transaction = false;
                        current_state = PCI_IDLE;
                        memset(&PCIData, 0, sizeof(PCIData));
                    }
                    else
                    {
                        // Create a reset indicator
                        TSeqData SeqData;
                        memset(&SeqData, 0, sizeof(SeqData));
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), 
                                "PCI Reset");
                        SeqData.seq_data.flags = 2; // Grey background for status
                        SeqData.seq_number = seq;
                        SeqDataVector.push_back(SeqData);
                    }
                    
                    previous_signals = signals;
                    continue; // Skip to next sample
                }
                
                // Process based on current state
                switch (current_state)
                {
                    case PCI_IDLE:
                        // Look for start of transaction (FRAME# asserted)
                        if ((signals & PCI_FRAME) == 0) // FRAME# is active low
                        {
                            // Begin a new transaction
                            in_transaction = true;
                            current_state = PCI_ADDRESS_PHASE;
                            
                            // Initialize transaction data
                            memset(&PCIData, 0, sizeof(PCIData));
                            PCIData.sequence_start = seq;
                            PCIData.command = extract_command(signals);
                            PCIData.address = extract_ad(signals) << 2; // Lower 2 bits always 0
                            
                            // Check for dual address cycle (64-bit address)
                            if (PCIData.command == PCI_CMD_DUAL_ADDR_CYCLE)
                            {
                                current_state = PCI_DUAL_ADDRESS_PHASE;
                            }
                            
                            // Check for special cycles
                            else if (PCIData.command == PCI_CMD_SPECIAL_CYCLE)
                            {
                                current_state = PCI_SPECIAL_CYCLE;
                            }
                            
                            // Check for config cycles
                            else if (is_config_transaction(PCIData.command))
                            {
                                current_state = PCI_CONFIG_CYCLE;
                                
                                // Decode device/function for configuration cycles
                                if (signals & PCI_IDSEL)
                                {
                                    // Type 0 configuration
                                    PCIData.is_type1_config = false;
                                    PCIData.device_num = (extract_ad(signals) >> 11) & 0x1F; // AD[15:11]
                                    PCIData.function_num = (extract_ad(signals) >> 8) & 0x7; // AD[10:8]
                                }
                                else
                                {
                                    // Type 1 configuration
                                    PCIData.is_type1_config = true;
                                    PCIData.device_num = (extract_ad(signals) >> 11) & 0x1F; // AD[15:11]
                                    PCIData.function_num = (extract_ad(signals) >> 8) & 0x7; // AD[10:8]
                                }
                            }
                            
                            // Check for arbitration
                            PCIData.req_asserted = (signals & PCI_REQ) == 0; // REQ# is active low
                            PCIData.gnt_asserted = (signals & PCI_GNT) == 0; // GNT# is active low
                            
                            // Check for lock
                            PCIData.lock_asserted = (signals & PCI_LOCK) == 0; // LOCK# is active low
                            
                            LogDebug(pctx, 1, "Starting transaction: CMD=%s, Addr=0x%08X", 
                                    get_command_string(PCIData.command), (uint32_t)PCIData.address);
                        }
                        // Check for arbitration (REQ# or GNT# changes)
                        else if (((signals & PCI_REQ) != (previous_signals & PCI_REQ)) ||
                                ((signals & PCI_GNT) != (previous_signals & PCI_GNT)))
                        {
                            TSeqData SeqData;
                            memset(&SeqData, 0, sizeof(SeqData));
                            
                            if ((signals & PCI_REQ) == 0 && (signals & PCI_GNT) != 0)
                            {
                                // REQ# asserted, GNT# not asserted
                                snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), 
                                        "Request for bus");
                            }
                            else if ((signals & PCI_REQ) != 0 && (signals & PCI_GNT) == 0)
                            {
                                // REQ# not asserted, GNT# asserted
                                snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), 
                                        "Bus grant without request");
                            }
                            else if ((signals & PCI_REQ) == 0 && (signals & PCI_GNT) == 0)
                            {
                                // Both REQ# and GNT# asserted
                                snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), 
                                        "Bus granted to requestor");
                                current_state = PCI_BUS_PARKING;
                            }
                            
                            SeqData.seq_data.flags = 2; // Grey background for status
                            SeqData.seq_number = seq;
                            SeqDataVector.push_back(SeqData);
                        }
                        break;
                        
                    case PCI_BUS_PARKING:
                        // In bus parking, waiting for transaction to start
                        if ((signals & PCI_FRAME) == 0) // FRAME# is active low
                        {
                            // Begin a new transaction
                            current_state = PCI_ADDRESS_PHASE;
                            
                            // Initialize transaction data
                            memset(&PCIData, 0, sizeof(PCIData));
                            PCIData.sequence_start = seq;
                            PCIData.command = extract_command(signals);
                            PCIData.address = extract_ad(signals) << 2; // Lower 2 bits always 0
                            
                            LogDebug(pctx, 1, "Starting transaction from bus parking: CMD=%s, Addr=0x%08X", 
                                    get_command_string(PCIData.command), (uint32_t)PCIData.address);
                        }
                        else if ((signals & PCI_REQ) != 0 || (signals & PCI_GNT) != 0)
                        {
                            // REQ# or GNT# deasserted, return to idle
                            current_state = PCI_IDLE;
                            
                            TSeqData SeqData;
                            memset(&SeqData, 0, sizeof(SeqData));
                            snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), 
                                    "End of bus parking");
                            SeqData.seq_data.flags = 2; // Grey background for status
                            SeqData.seq_number = seq;
                            SeqDataVector.push_back(SeqData);
                        }
                        break;
                        
                    case PCI_ADDRESS_PHASE:
                        // In the address phase, waiting for IRDY# to be asserted
                        if ((signals & PCI_IRDY) == 0) // IRDY# is active low
                        {
                            // Move to data phase
                            current_state = PCI_DATA_PHASE;
                            
                            // Check for target response
                            if ((signals & PCI_DEVSEL) == 0) // DEVSEL# is active low
                            {
                                // Target claimed the transaction
                                if ((signals & PCI_TRDY) == 0) // TRDY# is active low
                                {
                                    // Target ready, data phase can complete
                                    PCIData.byte_enables[PCIData.data_phase_count] = extract_byte_enables(signals);
                                    PCIData.data[PCIData.data_phase_count] = extract_ad(signals);
                                    PCIData.data_phase_count++;
                                    
                                    LogDebug(pctx, 2, "Data phase %d: Data=0x%08X, BE=0x%X", 
                                            PCIData.data_phase_count, PCIData.data[PCIData.data_phase_count-1],
                                            PCIData.byte_enables[PCIData.data_phase_count-1]);
                                }
                            }
                            else if (is_master_abort(signals))
                            {
                                // Master abort condition
                                PCIData.master_abort = true;
                                PCIData.completion_type = PCI_COMP_MASTER_ABORT;
                                current_state = PCI_COMPLETION_PHASE;
                                
                                LogDebug(pctx, 1, "Master Abort detected");
                            }
                        }
                        
                        // Check for early termination
                        if ((signals & PCI_FRAME) != 0 && (previous_signals & PCI_FRAME) == 0)
                        {
                            // FRAME# deasserted before data phase, indicates error
                            PCIData.completion_type = PCI_COMP_MASTER_ABORT;
                            current_state = PCI_COMPLETION_PHASE;
                            
                            LogDebug(pctx, 1, "Early FRAME# deassertion - error condition");
                        }
                        break;
                        
                    case PCI_DUAL_ADDRESS_PHASE:
                        // Second address phase for 64-bit addressing
                        PCIData.is_64bit = true;
                        PCIData.address |= ((uint64_t)extract_ad(signals) << 34); // Upper 32 bits of address
                        current_state = PCI_ADDRESS_PHASE;
                        
                        LogDebug(pctx, 1, "Dual address phase: 64-bit address=0x%016llX", PCIData.address);
                        break;
                        
                    case PCI_DATA_PHASE:
                        // Processing data phases
                        if ((signals & PCI_TRDY) == 0 && (signals & PCI_IRDY) == 0)
                        {
                            // Both TRDY# and IRDY# asserted, data transfer
                            PCIData.byte_enables[PCIData.data_phase_count] = extract_byte_enables(signals);
                            PCIData.data[PCIData.data_phase_count] = extract_ad(signals);
                            PCIData.data_phase_count++;
                            
                            LogDebug(pctx, 2, "Data phase %d: Data=0x%08X, BE=0x%X", 
                                    PCIData.data_phase_count, PCIData.data[PCIData.data_phase_count-1],
                                    PCIData.byte_enables[PCIData.data_phase_count-1]);
                        }
                        
                        // Check for target abort
                        if ((signals & PCI_DEVSEL) != 0 && (previous_signals & PCI_DEVSEL) == 0)
                        {
                            // DEVSEL# deasserted during transaction - target abort
                            PCIData.target_abort = true;
                            PCIData.completion_type = PCI_COMP_TARGET_ABORT;
                            current_state = PCI_COMPLETION_PHASE;
                            
                            LogDebug(pctx, 1, "Target Abort detected");
                        }
                        
                        // Check for retry
                        if ((signals & PCI_STOP) == 0 && (signals & PCI_TRDY) != 0)
                        {
                            // STOP# asserted and TRDY# not asserted - retry
                            PCIData.completion_type = PCI_COMP_RETRY;
                            current_state = PCI_COMPLETION_PHASE;
                            
                            LogDebug(pctx, 1, "Retry condition detected");
                        }
                        
                        // Check for disconnect
                        if ((signals & PCI_STOP) == 0 && (signals & PCI_TRDY) == 0)
                        {
                            // STOP# and TRDY# both asserted - disconnect with data
                            PCIData.completion_type = PCI_COMP_DISCONNECT;
                            
                            if (PCIData.data_phase_count >= 16)
                            {
                                // Maximum data phases reached
                                current_state = PCI_COMPLETION_PHASE;
                            }
                            
                            LogDebug(pctx, 1, "Disconnect condition detected");
                        }
                        
                        // Check for transaction end
                        if ((signals & PCI_FRAME) != 0 && (signals & PCI_IRDY) == 0)
                        {
                            // FRAME# deasserted and IRDY# asserted - last data phase
                            if ((signals & PCI_TRDY) == 0)
                            {
                                // TRDY# also asserted, completing last data phase
                                current_state = PCI_COMPLETION_PHASE;
                                PCIData.sequence_end = seq;
                                
                                // Determine burst type
                                if (PCIData.data_phase_count == 1)
                                {
                                    PCIData.burst_type = PCI_BURST_SINGLE;
                                }
                                else if (is_burst_transaction(PCIData.command))
                                {
                                    if (PCIData.command == PCI_CMD_MEM_READ_LINE || 
                                        PCIData.command == PCI_CMD_MEM_WRITE_AND_INV)
                                    {
                                        PCIData.burst_type = PCI_BURST_LINE;
                                        PCIData.is_cache_line = true;
                                    }
                                    else if (PCIData.command == PCI_CMD_MEM_READ_MULTIPLE)
                                    {
                                        PCIData.burst_type = PCI_BURST_MULTIPLE;
                                    }
                                }
                                else
                                {
                                    PCIData.burst_type = PCI_BURST_CONTINUOUS;
                                }
                                
                                LogDebug(pctx, 1, "Transaction completed: %d data phases", PCIData.data_phase_count);
                            }
                        }
                        
                        // Check for parity error
                        if ((signals & PCI_PERR) == 0) // PERR# is active low
                        {
                            PCIData.parity_error = true;
                            
                            LogDebug(pctx, 1, "Parity error detected");
                        }
                        
                        // Check for system error
                        if ((signals & PCI_SERR) == 0) // SERR# is active low
                        {
                            PCIData.system_error = true;
                            
                            LogDebug(pctx, 1, "System error detected");
                        }
                        break;
                        
                    case PCI_CONFIG_CYCLE:
                        // Configuration cycle - similar to address phase but with special handling
                        if ((signals & PCI_IRDY) == 0) // IRDY# is active low
                        {
                            // Configuration command, now entering data phase
                            current_state = PCI_DATA_PHASE;
                            
                            LogDebug(pctx, 1, "Config cycle: Dev=%d Func=%d Type=%d", 
                                    PCIData.device_num, PCIData.function_num, PCIData.is_type1_config ? 1 : 0);
                        }
                        break;
                        
                    case PCI_SPECIAL_CYCLE:
                        // Special cycle - broadcast to all devices
                        current_state = PCI_DATA_PHASE;
                        
                        LogDebug(pctx, 1, "Special cycle: Message=0x%08X", (uint32_t)PCIData.address);
                        break;
                        
                    case PCI_COMPLETION_PHASE:
                        // Transaction completed
                        if ((signals & PCI_IRDY) != 0 && (signals & PCI_FRAME) != 0)
                        {
                            // Both IRDY# and FRAME# deasserted, bus is idle
                            PCIData.sequence_end = seq;
                            
                            // Now create a sequence entry for this transaction
                            TSeqData SeqData;
                            memset(&SeqData, 0, sizeof(SeqData));
                            
                            // Format transaction details into text
                            format_transaction(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), PCIData);
                            
                            // Set flags based on transaction status
                            if (PCIData.parity_error || PCIData.system_error || 
                                PCIData.master_abort || PCIData.target_abort ||
                                PCIData.completion_type == PCI_COMP_RETRY || 
                                PCIData.completion_type == PCI_COMP_TARGET_ABORT)
                            {
                                SeqData.seq_data.flags = 4; // Red background for errors
                            }
                            else if (PCIData.completion_type == PCI_COMP_DISCONNECT)
                            {
                                SeqData.seq_data.flags = 8; // Yellow background for warnings
                            }
                            else
                            {
                                SeqData.seq_data.flags = 1; // Normal display
                            }
                            
                            SeqData.seq_number = PCIData.sequence_start;
                            SeqDataVector.push_back(SeqData);
                            
                            // Save the transaction for future reference
                            PCITransactions.push_back(PCIData);
                            
                            // Reset for next transaction
                            in_transaction = false;
                            current_state = PCI_IDLE;
                            memset(&PCIData, 0, sizeof(PCIData));
                            
                            LogDebug(pctx, 1, "Transaction completed, returning to idle state");
                        }
                        break;
                }
                
                // Check for interrupt assertions
                if ((signals & (PCI_INTA | PCI_INTB | PCI_INTC | PCI_INTD)) != 
                    (previous_signals & (PCI_INTA | PCI_INTB | PCI_INTC | PCI_INTD)))
                {
                    // Interrupt state changed
                    TSeqData SeqData;
                    memset(&SeqData, 0, sizeof(SeqData));
                    
                    if ((signals & PCI_INTA) == 0 && (previous_signals & PCI_INTA) != 0)
                    {
                        // INTA# asserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTA# Asserted");
                    }
                    else if ((signals & PCI_INTB) == 0 && (previous_signals & PCI_INTB) != 0)
                    {
                        // INTB# asserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTB# Asserted");
                    }
                    else if ((signals & PCI_INTC) == 0 && (previous_signals & PCI_INTC) != 0)
                    {
                        // INTC# asserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTC# Asserted");
                    }
                    else if ((signals & PCI_INTD) == 0 && (previous_signals & PCI_INTD) != 0)
                    {
                        // INTD# asserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTD# Asserted");
                    }
                    else if ((signals & PCI_INTA) != 0 && (previous_signals & PCI_INTA) == 0)
                    {
                        // INTA# deasserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTA# Deasserted");
                    }
                    else if ((signals & PCI_INTB) != 0 && (previous_signals & PCI_INTB) == 0)
                    {
                        // INTB# deasserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTB# Deasserted");
                    }
                    else if ((signals & PCI_INTC) != 0 && (previous_signals & PCI_INTC) == 0)
                    {
                        // INTC# deasserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTC# Deasserted");
                    }
                    else if ((signals & PCI_INTD) != 0 && (previous_signals & PCI_INTD) == 0)
                    {
                        // INTD# deasserted
                        snprintf(SeqData.seq_data.text, sizeof(SeqData.seq_data.text), "INTD# Deasserted");
                    }
                    
                    SeqData.seq_data.flags = 2; // Grey background for status events
                    SeqData.seq_number = seq;
                    SeqDataVector.push_back(SeqData);
                    
                    LogDebug(pctx, 1, "Interrupt state change detected");
                }
            }
            
            // Save signals for edge detection
            previous_signals = signals;
        }
        
        // Post-processing: finalize text pointers for all sequences
        for (it = SeqDataVector.begin(); it != SeqDataVector.end(); ++it)
        {
            it->seq_data.textp = it->seq_data.text;
            it->seq_data.text2 = it->seq_data.text2_buf;
        }
        
        LogDebug(pctx, 0, "Found %d PCI transactions", PCITransactions.size());
    }
    
    // Return the requested sequence
    for (it = SeqDataVector.begin(); it != SeqDataVector.end(); ++it)
    {
        if (it->seq_number == initseq)
        {
            seqinfo = &(it->seq_data);
            LogDebug(pctx, 9, "%s: seq: %d text: [%s]", "ParseSeq", initseq, seqinfo->textp);
            break;
        }
    }
    
    return seqinfo;
}

int ParseMarkNext(struct pctx *pctx, int seq, int a3)
{
    LogDebug(pctx, 9, "%s: sequence %d, a3 %d", "ParseMarkNext", seq, a3);
    return 0;
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
            return (int)"PCI";
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
    PCITransactions.clear();
    processing_done = 0;
    
    if (bus >= ARRAY_SIZE(businfo))
        return NULL;
    
    return (struct businfo*) businfo + bus;
}

struct groupinfo *ParseGroupInfo(struct pctx *pctx, uint16_t group)
{
    LogDebug(pctx, 9, "%s: %08x", "ParseGroupInfo", group);
    
    if (group >= ARRAY_SIZE(groupinfo))
        return NULL;
    
    return (struct groupinfo*) groupinfo + group;
}

struct modeinfo *ParseModeInfo(struct pctx *pctx, uint16_t mode)
{
    LogDebug(pctx, 9, "%s: %d", "ParseModeInfo", mode);
    
    if (mode >= ARRAY_SIZE(modeinfo))
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
                // BUS_WIDTH setting
                set_bus_width = value;
                break;
            case 1:
                // BUS_SPEED setting
                set_bus_speed = value;
                break;
            case 2:
                // ARB_MODE setting
                set_arb_mode = value;
                break;
            case 3:
                // CACHELINE setting
                set_cache_line_size = value;
                break;
            case 4:
                // LATENCY setting
                set_latency = value;
                break;
            case 5:
                // RETRY_POLICY setting
                set_retry_policy = value;
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
                // BUS_WIDTH setting
                value = set_bus_width;
                break;
            case 1:
                // BUS_SPEED setting
                value = set_bus_speed;
                break;
            case 2:
                // ARB_MODE setting
                value = set_arb_mode;
                break;
            case 3:
                // CACHELINE setting
                value = set_cache_line_size;
                break;
            case 4:
                // LATENCY setting
                value = set_latency;
                break;
            case 5:
                // RETRY_POLICY setting
                value = set_retry_policy;
                break;
            default:
                value = 0;
                break;
        }
    }
    
    LogDebug(pctx, 9, "%s: Settings updated", "ParseModeGetPut");
    return value;
}

int ParseDisasmReinit(struct pctx *pctx, int request)
{
    // Initialize parsing
    LogDebug(pctx, 9, "%s: pctx %08x, request=%d", "ParseDisasmReinit", pctx, request);
    return 1;
}