
# Technical Notes

### ISA Bus Timing

The 8/16-bit ISA bus uses a state-based protocol:
- **T1**: Address phase - addresses latched on falling edge of ALE
- **T2**: Data phase begins - command signals active
- **TW**: Wait states - inserted when IOCHRDY is inactive
- **T3**: Completion phase - data transfer and command deassertion

### Bus Width Detection

The analyzer automatically detects 8-bit vs 16-bit transfers based on:
- SBHE# signal (active for 16-bit transfers)
- Address alignment (odd addresses typically use 8-bit transfers)

### Advanced Error Detection

When Advanced Error Detection is enabled, the analyzer checks for:
- Excessive wait states
- Protocol violations (improper signal sequencing)
- IOCHK# errors
- Bus timeouts
- Incomplete transactions

## ISA Bus Architecture Overview

The Industry Standard Architecture (ISA) bus is a computer bus standard that was widely used in IBM PC/XT, PC/AT, and compatible computers. The ISA bus provides a means for expansion cards to communicate with the CPU, memory, and other system components.

### Historical Context

- **1981**: Original 8-bit ISA bus introduced with the IBM PC (4.77 MHz)
- **1984**: Extended to 16-bit with the IBM PC/AT (6-8 MHz)
- **Late 1980s**: Enhanced implementations with higher clock rates (8-12 MHz)
- **1990s**: Continued use despite introduction of newer bus standards
- **Early 2000s**: Phased out in favor of PCI and other modern buses

### ISA Bus Types

1. **8-bit ISA**: Original PC/XT bus with 62-pin connector
   - 8-bit data bus
   - 20-bit address bus
   - Up to 1MB addressable memory
   - DMA channels 0-3
   - IRQ lines 2-7

2. **16-bit ISA**: Extended PC/AT bus with 98-pin connector
   - 16-bit data bus
   - 24-bit address bus
   - Up to 16MB addressable memory
   - Additional DMA channels (5-7)
   - Additional IRQ lines (9-15)

## ISA Bus Protocol

### Bus Cycles

The ISA bus uses a state-based protocol with the following states:

1. **T1 (Address Phase)**:
   - Address is placed on the address bus
   - ALE (Address Latch Enable) is asserted to latch the address
   - Command signals remain inactive

2. **T2 (Command Phase)**:
   - Command signals (IOR#, IOW#, MEMR#, MEMW#) become active
   - For write operations: Data is placed on the data bus
   - For read operations: Target prepares to drive data bus

3. **TW (Wait States)**: Optional
   - IOCHRDY signal can be deasserted to insert wait states
   - Command signals remain active
   - Multiple wait states may be inserted

4. **T3 (Data Phase)**:
   - For read operations: Data is present on the data bus
   - For write operations: Data is latched by the target
   - Command signals are deasserted
   - Bus cycle completes

### Signal Descriptions

#### Control Signals

- **BCLK**: Bus Clock, system timing reference (4.77-12 MHz)
- **ALE**: Address Latch Enable, used to demultiplex address/data on the bus
- **IOR#**: I/O Read command, active low
- **IOW#**: I/O Write command, active low
- **MEMR#**: Memory Read command, active low
- **MEMW#**: Memory Write command, active low
- **REFRESH#**: Memory Refresh cycle, active low
- **IOCHRDY**: I/O Channel Ready, deasserted to insert wait states
- **SBHE#**: System Bus High Enable, active for 16-bit transfers
- **AEN**: Address Enable, isolates I/O devices during DMA
- **MASTER#**: Bus Master, allows devices to take control of the bus
- **IOCHK#**: I/O Channel Check, system error indicator
- **RESET**: System Reset

#### Address/Data Buses

- **A0-A19**: Address lines for 8-bit ISA (20-bit addressing)
- **A0-A23**: Address lines for 16-bit ISA (24-bit addressing)
- **D0-D7**: Data lines for 8-bit transfers
- **D0-D15**: Data lines for 16-bit transfers

#### DMA Signals

- **DRQn**: DMA Request lines (channels 0-7)
- **DACKn#**: DMA Acknowledge lines (channels 0-7)
- **TC**: Terminal Count, indicates end of DMA transfer

#### Interrupt Signals

- **IRQ2-IRQ7**: Interrupt Request lines (8-bit ISA)
- **IRQ9-IRQ15**: Additional Interrupt Request lines (16-bit ISA)

### ISA Transaction Types

#### I/O Operations

1. **I/O Read (8-bit)**:
   - IOR# signal asserted
   - A0-A15 specify I/O port address
   - D0-D7 return data

2. **I/O Read (16-bit)**:
   - IOR# signal asserted
   - SBHE# signal asserted
   - A0-A15 specify I/O port address (typically even-aligned)
   - D0-D15 return data

3. **I/O Write (8-bit)**:
   - IOW# signal asserted
   - A0-A15 specify I/O port address
   - D0-D7 contain data to write

4. **I/O Write (16-bit)**:
   - IOW# signal asserted
   - SBHE# signal asserted
   - A0-A15 specify I/O port address (typically even-aligned)
   - D0-D15 contain data to write

#### Memory Operations

1. **Memory Read (8-bit)**:
   - MEMR# signal asserted
   - A0-A23 specify memory address
   - D0-D7 return data

2. **Memory Read (16-bit)**:
   - MEMR# signal asserted
   - SBHE# signal asserted
   - A0-A23 specify memory address (typically even-aligned)
   - D0-D15 return data

3. **Memory Write (8-bit)**:
   - MEMW# signal asserted
   - A0-A23 specify memory address
   - D0-D7 contain data to write

4. **Memory Write (16-bit)**:
   - MEMW# signal asserted
   - SBHE# signal asserted
   - A0-A23 specify memory address (typically even-aligned)
   - D0-D15 contain data to write

#### DMA Operations

1. **DMA Read**:
   - DACKn# signal asserted for selected DMA channel
   - MEMR# or IOR# asserted depending on transfer type
   - Address generated by DMA controller
   - Data transferred through data bus

2. **DMA Write**:
   - DACKn# signal asserted for selected DMA channel
   - MEMW# or IOW# asserted depending on transfer type
   - Address generated by DMA controller
   - Data transferred through data bus

3. **Terminal Count**:
   - TC signal asserted at end of DMA block transfer
   - Signals completion of programmed transfer count

#### Refresh Operations

- **Memory Refresh**:
   - REFRESH# signal asserted
   - Address lines contain refresh row address
   - Used to refresh dynamic RAM

### Timing Specifications

Typical timing parameters for standard ISA bus (8 MHz):

- **Clock Period**: 125 ns (8 MHz)
- **Address Setup Time**: 30 ns min before command active
- **Command Pulse Width**: 125 ns min (1 clock cycle)
- **Data Setup Time (Write)**: 30 ns min before command inactive
- **Data Hold Time (Write)**: 0 ns min after command inactive
- **Data Access Time (Read)**: 83 ns max after command active
- **Data Hold Time (Read)**: 0 ns min after command inactive
- **Wait State Duration**: 125 ns per wait state (1 clock cycle)

# Protocol Analyzer Implementation

### State Machine Implementation

The ISA Protocol Analyzer implements a complete state machine that tracks the following states:

1. **ISA_STATE_IDLE**: No active transaction
2. **ISA_STATE_T1**: Address phase of bus cycle
3. **ISA_STATE_T2**: Command phase of bus cycle
4. **ISA_STATE_TW**: Wait states
5. **ISA_STATE_T3**: Data phase of bus cycle
6. **ISA_STATE_DMA_ACTIVE**: DMA cycle in progress
7. **ISA_STATE_REFRESH**: Memory refresh cycle

### Address Decoding

The analyzer implements full address decoding with support for:

- **16-bit addressing**: Standard I/O and memory addressing (64KB I/O, 64KB memory)
- **20-bit addressing**: Extended memory addressing (1MB)
- **24-bit addressing**: Enhanced memory addressing (16MB)

### Data Width Detection

The analyzer automatically detects 8-bit vs 16-bit transfers based on:

1. **SBHE# Signal**: System Bus High Enable indicates 16-bit transfer
2. **Address Alignment**: Even addresses typically use 16-bit transfers when supported
3. **Data Bus Activity**: Monitoring both halves of the data bus

### Wait State Tracking

The analyzer tracks wait states inserted during bus cycles:

1. **IOCHRDY Monitoring**: Detects when target deasserts IOCHRDY
2. **Clock Counting**: Counts additional clock cycles during command phase
3. **Timeout Detection**: Flags excessive wait states as potential errors

### DMA Handling

Complete DMA support includes:

1. **Channel Detection**: Identifies active DMA channel via DACKn# signals
2. **Transfer Type**: Determines read vs write based on command signals
3. **Terminal Count**: Detects TC signal assertion at end of block transfer
4. **Address Tracking**: Captures DMA address information when available

### Interrupt Detection

The analyzer monitors all ISA interrupt lines:

1. **Edge Detection**: Identifies rising edges on IRQ lines
2. **Activity Logging**: Records interrupt activity separate from bus cycles
3. **Correlation**: Associates interrupts with subsequent bus activity when possible

### Advanced Error Detection

When configured for advanced error detection, the analyzer checks for:

1. **Timing Violations**: Command pulse widths, setup times, hold times
2. **Protocol Errors**: Improper signal sequencing
3. **Bus Contention**: Multiple devices driving the bus simultaneously
4. **Timeouts**: Bus cycles that don't complete within expected time
5. **IOCHK# Errors**: System errors signaled via IOCHK#

## Implementation Challenges

### Signal Sampling Considerations

1. **Asynchronous Sampling**: Since the logic analyzer may not be synchronized to the ISA clock, care must be taken to properly detect edges and state transitions.

2. **Sample Rate**: The analyzer uses oversampling (at least 5x the ISA bus frequency) to ensure accurate edge detection and timing measurements.

3. **Signal Threshold**: TTL-level thresholds (typically 1.5V) are used, which may need adjustment for noisy environments.

### Edge Detection

The analyzer implements robust edge detection:

1. **Double-Buffering**: Previous and current signal states are compared
2. **Glitch Filtering**: Very short pulses can be filtered to avoid false triggers
3. **Clock Edge Alignment**: State transitions are aligned to BCLK edges
4. **Hysteresis**: To prevent false triggering on noisy signals

### Transaction Correlation

The analyzer correlates related events:

1. **Address-Data Pairing**: Associates address phase with corresponding data phase
2. **Interrupt-Handler Tracking**: Correlates interrupts with subsequent service routines
3. **DMA Transfer Tracking**: Follows complete DMA transactions across multiple bus cycles

## Protocol Analysis Algorithms

### Bus Cycle Detection

The analyzer uses the following algorithm to detect bus cycles:

1. Monitor control signals (IOR#, IOW#, MEMR#, MEMW#, ALE)
2. Detect start of cycle via ALE assertion or command signal activation
3. Track state progression through T1, T2, TW, and T3 states
4. Detect end of cycle via command signal deactivation
5. Extract address, data, and timing information

### DMA Cycle Detection

DMA cycles are detected through:

1. Monitoring DACKn# signals for assertion
2. Tracking associated command signals (MEMR#, MEMW#, IOR#, IOW#)
3. Following TC signal for end of block transfers
4. Calculating transfer size and timing

### Address Reconstruction

For multiplexed address/data bus systems, address reconstruction uses:

1. Latching address bits during ALE assertion
2. Combining address bits from multiple sources (A0-A23, LA17-LA23)
3. Applying appropriate address masking based on configured address width

### Wait State Calculation

Wait state calculation uses the algorithm:

1. Start counting at T2 state
2. Monitor IOCHRDY signal
3. Count clock cycles while IOCHRDY is deasserted
4. Apply timing model based on selected timing mode
5. Calculate total wait states inserted

## Performance Optimizations

The analyzer implements several optimizations:

1. **Efficient State Machine**: Minimizes state transitions and condition checks
2. **Look-ahead Parsing**: Pre-processes signals to identify potential transactions
3. **Intelligent Signal Filtering**: Focuses on relevant signals for current transaction type
4. **Smart Buffer Management**: Efficiently manages memory for captured sequences
5. **Incremental Processing**: Processes data in chunks to avoid large memory allocations

## Technical References

For further information, consult these technical references:

1. IBM PC/AT Technical Reference Manual
2. ISA Bus Specification (IEEE P996)
3. Intel 8237 DMA Controller Datasheet
4. Intel 8259 Programmable Interrupt Controller Datasheet
5. ISA System Architecture (MindShare Architecture Series)
