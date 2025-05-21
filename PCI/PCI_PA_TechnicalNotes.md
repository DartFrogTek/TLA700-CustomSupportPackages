
# PCI Protocol Analyzer: Technical Notes

## PCI Bus Architecture Overview

Peripheral Component Interconnect (PCI) is a computer bus standard developed by Intel in 1992. It provides a high-performance, processor-independent local bus for connecting peripherals directly to the computer system.

### Historical Context

- **1992**: PCI 1.0 specification released by Intel
- **1993**: PCI 2.0 introduced with 3.3V signaling option
- **1995**: PCI 2.1 published with clarifications and improvements
- **1998**: PCI 2.2 added features like power management
- **2002**: PCI 2.3 released (final version of conventional PCI)
- **Mid-1990s to early 2000s**: PCI's heyday in desktop computers
- **2004**: PCI Express introduced as replacement
- **2010s**: PCI largely displaced by PCI Express in new systems

### PCI Bus Types

1. **32-bit PCI at 33 MHz**: 
   - Original implementation
   - 32-bit multiplexed address/data bus
   - 33 MHz clock frequency
   - 132 MB/s peak bandwidth
   - 5V or 3.3V signaling

2. **32-bit PCI at 66 MHz**:
   - Enhanced implementation
   - 32-bit multiplexed address/data bus
   - 66 MHz clock frequency
   - 264 MB/s peak bandwidth
   - 3.3V signaling only

3. **64-bit PCI at 33 MHz**:
   - Extended data width
   - 64-bit multiplexed address/data bus
   - 33 MHz clock frequency
   - 264 MB/s peak bandwidth
   - 5V or 3.3V signaling

4. **64-bit PCI at 66 MHz**:
   - Highest performance conventional PCI
   - 64-bit multiplexed address/data bus
   - 66 MHz clock frequency
   - 528 MB/s peak bandwidth
   - 3.3V signaling only

5. **PCI-X**:
   - Server-oriented PCI extension
   - Higher frequencies (66, 100, 133 MHz)
   - Enhanced protocol features
   - Up to 1064 MB/s bandwidth

## PCI Bus Timing

The PCI bus uses a synchronous protocol with the following timings:
- **Clock**: All signals are synchronized to the rising edge of CLK
- **Setup Time**: All inputs must be stable at least 7ns before CLK rising edge
- **Hold Time**: All inputs must remain stable at least 0ns after CLK rising edge
- **Output Delay**: Outputs must be driven within 11ns after CLK rising edge

## Bus Transaction Timing

PCI bus transactions follow this sequence:
- **Address Phase**: Single clock cycle where address and command are driven
- **Data Phase(s)**: One or more clock cycles where data is transferred
- **Turnaround Cycle**: Required for changing bus direction (read to write, etc.)

## Advanced Error Detection

When Advanced Error Detection is enabled, the analyzer checks for:
- Protocol violations (improper signal sequencing)
- Timing violations (setup/hold time errors)
- Parity errors (PAR, PAR64)
- Bus timeouts (excessive wait states)
- Incomplete transactions
- Invalid command sequences

## PCI Bus Protocol in Detail

### Bus Cycles

The PCI bus operates using a state-based protocol with these phases:

1. **Address Phase**:
   - Initiator asserts FRAME# to start transaction
   - Address placed on AD[31:0] or AD[63:0]
   - Command placed on C/BE[3:0]#
   - Targets sample address and command on rising edge of CLK

2. **Data Phase**:
   - Initiator drives or samples data on AD lines
   - Initiator drives byte enables on C/BE lines
   - Initiator asserts IRDY# when ready
   - Target asserts TRDY# when ready
   - Data transfer occurs when both IRDY# and TRDY# are active
   - Multiple data phases may occur in one transaction

3. **Turnaround Cycle**:
   - Required when bus ownership changes
   - Prevents bus contention
   - One clock cycle where no device drives the bus

4. **Completion Phase**:
   - Initiator deasserts FRAME# during last data phase
   - Both IRDY# and TRDY# must be active for final transfer
   - Bus returns to idle state after completion

### Signal Details

#### Clock and Reset

- **CLK**: System clock signal
  - All PCI transactions are synchronous to this signal
  - Standard frequencies are 33 MHz and 66 MHz
  - All signals sampled on rising edge of CLK

- **RST#**: Reset signal
  - Resets all PCI devices when asserted
  - Must be asserted for at least 100 ms during power-up
  - All output signals are tristated during reset

#### Address/Data Signals

- **AD[31:0]**: Address/Data Bus (32-bit)
  - Multiplexed for address and data
  - During address phase: carries target address
  - During data phase: carries data being transferred

- **AD[63:32]**: Extended Address/Data Bus (64-bit)
  - Used for 64-bit PCI transfers
  - Must be driven to valid logic levels even when not used

- **C/BE[3:0]#**: Command/Byte Enable
  - Multiplexed for command and byte enables
  - During address phase: carries bus command
  - During data phase: indicates active data bytes

- **C/BE[7:4]#**: Extended Command/Byte Enable
  - Used for 64-bit PCI transfers
  - Must be driven to valid logic levels even when not used

#### Control Signals

- **FRAME#**: Cycle Frame
  - Indicates beginning and duration of transaction
  - Asserted at start of address phase
  - Deasserted during last data phase

- **IRDY#**: Initiator Ready
  - Indicates initiator's ability to complete current data phase
  - Must be deasserted during address phase
  - Data transfer occurs when both IRDY# and TRDY# are asserted

- **TRDY#**: Target Ready
  - Indicates target's ability to complete current data phase
  - Data transfer occurs when both IRDY# and TRDY# are asserted
  - Target may insert wait states by deasserting TRDY#

- **STOP#**: Stop
  - Allows target to request transaction termination
  - Normal Disconnect: STOP# and TRDY# both asserted
  - Retry: STOP# asserted, TRDY# deasserted
  - Target Abort: STOP# asserted, DEVSEL# deasserted

- **DEVSEL#**: Device Select
  - Indicates that a target has decoded its address
  - Must be asserted within 3 clocks of FRAME# assertion
  - Timing classified as: Fast (1 clock), Medium (2 clocks), Slow (3 clocks)
  - Master Abort occurs if DEVSEL# is not asserted within 5 clocks

#### Error Signals

- **PAR**: Parity
  - Even parity for AD[31:0] and C/BE[3:0]#
  - Valid one clock after address phase or data phase
  - Used for data integrity verification

- **PAR64**: 64-bit Parity
  - Even parity for AD[63:32] and C/BE[7:4]#
  - Only used during 64-bit transfers

- **PERR#**: Parity Error
  - Asserted when data parity error detected
  - Used only for data phases, not address phase
  - Typically causes transaction retry

- **SERR#**: System Error
  - Asserted for address parity errors
  - Also used for other critical system errors
  - May trigger system-level error handling

#### 64-bit Extension Signals

- **REQ64#**: Request 64-bit Transfer
  - Asserted by initiator during address phase
  - Indicates desire to perform 64-bit transfer

- **ACK64#**: Acknowledge 64-bit Transfer
  - Asserted by target during address phase
  - Indicates capability to perform 64-bit transfer

#### Arbitration Signals

- **REQ#**: Request
  - Asserted by master to request bus ownership
  - Point-to-point signal between master and arbiter
  - Each master has its own REQ# signal

- **GNT#**: Grant
  - Asserted by arbiter to grant bus ownership
  - Point-to-point signal between arbiter and master
  - Each master has its own GNT# signal

### PCI Transaction Types

#### Memory Transactions

1. **Memory Read (0110)**:
   - Single data phase read from memory
   - Used for single word access

2. **Memory Write (0111)**:
   - Single data phase write to memory
   - Used for single word access

3. **Memory Read Multiple (1100)**:
   - Burst read with multiple data phases
   - Used for multiple cacheline transfers

4. **Memory Read Line (1110)**:
   - Burst read of a single cacheline
   - Optimized for cache line fill

5. **Memory Write and Invalidate (1111)**:
   - Burst write of entire cacheline
   - Guarantees complete cacheline write

#### I/O Transactions

1. **I/O Read (0010)**:
   - Read from I/O address space
   - Typically single data phase
   - Limited to 32-bit addressing

2. **I/O Write (0011)**:
   - Write to I/O address space
   - Typically single data phase
   - Limited to 32-bit addressing

#### Configuration Transactions

1. **Configuration Read (1010)**:
   - Read from configuration space
   - Accesses device configuration registers
   - Type 0: Direct access to local device
   - Type 1: Forwarded access via bridge

2. **Configuration Write (1011)**:
   - Write to configuration space
   - Accesses device configuration registers
   - Type 0: Direct access to local device
   - Type 1: Forwarded access via bridge

#### Special Transactions

1. **Interrupt Acknowledge (0000)**:
   - Special cycle for interrupt acknowledgment
   - Used with Intel x86 architecture

2. **Special Cycle (0001)**:
   - Broadcast message to all PCI devices
   - No specific target device
   - Used for system-wide notifications

3. **Dual Address Cycle (1101)**:
   - Used for 64-bit addressing
   - Two consecutive address phases

## Protocol Analyzer Implementation

### State Machine Implementation

The PCI Protocol Analyzer implements a complete state machine that tracks the following states:

1. **PCI_IDLE**: No active transaction
2. **PCI_ADDRESS_PHASE**: Address phase of bus cycle
3. **PCI_TURNAROUND**: Turnaround phase after address
4. **PCI_DATA_PHASE**: Data phase of bus cycle
5. **PCI_DUAL_ADDRESS_PHASE**: Secondary address phase for 64-bit addressing
6. **PCI_CONFIG_CYCLE**: Configuration cycle (Type 0 or Type 1)
7. **PCI_SPECIAL_CYCLE**: Special cycle broadcast

### Transaction Tracking

The analyzer tracks all aspects of PCI transactions:

1. **Command Decoding**:
   - Decodes C/BE[3:0]# during address phase to determine transaction type
   - Interprets address phase signals to identify target and addressing mode

2. **Data Phase Tracking**:
   - Tracks multiple data phases within a single transaction
   - Monitors IRDY# and TRDY# for wait state insertion
   - Calculates total throughput and efficiency

3. **Burst Transfer Analysis**:
   - Identifies burst transfer types (linear, cacheline, etc.)
   - Calculates burst length and transfer size
   - Monitors for premature termination

4. **Error Detection**:
   - Verifies parity signals (PAR and PAR64)
   - Detects protocol violations
   - Identifies PERR# and SERR# assertions
   - Detects Master and Target Aborts

### Configuration Space Access

The analyzer provides detailed analysis of configuration accesses:

1. **Type 0/Type 1 Differentiation**:
   - Identifies Type 0 (local) vs. Type 1 (forwarded) configuration cycles
   - Decodes device and function numbers
   - Maps register accesses to standard PCI configuration space

2. **Configuration Register Tracking**:
   - Decodes standard configuration registers
   - Tracks power management state changes
   - Monitors interrupt line changes
   - Follows resource allocation

### Advanced Features

1. **Transaction Latency Measurement**:
   - Measures time from address phase to first data phase
   - Tracks total transaction completion time
   - Identifies sources of latency (initiator or target)

2. **Arbitration Analysis**:
   - Tracks REQ#/GNT# signal pairs
   - Measures time from request to grant
   - Analyzes fairness of bus allocation

3. **Resource Conflict Detection**:
   - Identifies overlapping resource assignments
   - Monitors for improper configuration
   - Detects potential address conflicts

4. **Power Management Tracking**:
   - Monitors device power state transitions
   - Tracks power management events
   - Verifies proper power state handling

## Performance Optimization Techniques

The analyzer includes several optimizations:

1. **Efficient Signal Processing**:
   - Uses bit-field operations for fast signal extraction
   - Optimized state machine transitions
   - Minimal memory allocation during analysis

2. **Smart Filtering**:
   - Configurable filtering to focus on specific transaction types
   - Address range filtering to isolate specific memory regions
   - Device/function filtering for configuration space analysis

3. **Hierarchical Analysis**:
   - Multi-level transaction decoding
   - Progressive detail expansion
   - Context-sensitive display options

4. **Pattern Matching**:
   - Identifies common transaction patterns
   - Correlates related transactions
   - Groups transactions by function

## Technical References

For further information, consult these technical references:

1. PCI Local Bus Specification, Revision 2.3, PCI Special Interest Group, 2002
2. PCI System Architecture (MindShare Architecture Series), Tom Shanley & Don Anderson
3. PCI Hardware and Software: Architecture and Design, Edward Solari & George Willse
4. PCI & PCI-X Hardware and Software: Architecture and Design, Edward Solari
5. PCI Express System Architecture, Ravi Budruk, Don Anderson, & Tom Shanley