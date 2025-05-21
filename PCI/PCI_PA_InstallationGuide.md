# PCI Protocol Analyzer for TLA700: Installation Guide

## 1. Introduction to PCI

The Peripheral Component Interconnect (PCI) is a high-performance local bus architecture developed by Intel in 1992. This protocol analyzer supports the PCI Local Bus Specification, Revision 2.3, which defines the electrical, mechanical, and logical requirements for the PCI bus.

PCI buses typically operate at 33 MHz or 66 MHz with a 32-bit or 64-bit data path, providing peak bandwidths of up to 532 MB/s (66 MHz, 64-bit). The PCI bus architecture is designed for high throughput with low latency, synchronous operation, and multiplexed address/data lines that reduce pin count.

Before PCI, computer expansion slots used standards like ISA (Industry Standard Architecture) and EISA (Extended ISA). PCI quickly gained popularity due to its higher performance and Plug-and-Play capability, which simplified device installation and configuration. PCI was widely deployed from the mid-1990s through the early 2000s, becoming the standard expansion bus for desktop computers, servers, and workstations.

## 2. PCI Technical Reference

### 2.1 PCI Bus Signals

PCI is a multiplexed bus design that uses the same physical pins for both address and data, reducing connector size. Key PCI bus signals include:

**System Signals:**
- **CLK**: System clock signal (33 MHz or 66 MHz)
- **RST#**: System reset signal (active low)

**Address/Data Signals:**
- **AD[31:0]**: Multiplexed address/data lines for 32-bit operation
- **AD[63:32]**: Extended address/data lines for 64-bit operation
- **C/BE[3:0]#**: Command/Byte Enable signals (multiplexed)
- **C/BE[7:4]#**: Extended Command/Byte Enable signals (64-bit mode)
- **PAR**: Parity signal for AD[31:0] and C/BE[3:0]#
- **PAR64**: Parity signal for AD[63:32] and C/BE[7:4]# (64-bit mode)

**Interface Control Signals:**
- **FRAME#**: Cycle frame indicator (active low)
- **IRDY#**: Initiator Ready (active low)
- **TRDY#**: Target Ready (active low)
- **STOP#**: Target request to stop (active low)
- **DEVSEL#**: Device Select (active low)
- **IDSEL**: Initialization Device Select (chip select for configuration)

**Arbitration Signals:**
- **REQ#**: Bus Request (active low)
- **GNT#**: Bus Grant (active low)

**Error Reporting Signals:**
- **PERR#**: Data Parity Error (active low)
- **SERR#**: System Error (active low)

**Additional Control Signals:**
- **LOCK#**: Lock operation (exclusive resource access, active low)
- **INTA#, INTB#, INTC#, INTD#**: Interrupt request lines (active low)
- **SBO#**: Snoop Backoff
- **SDONE**: Snoop Done

### 2.2 PCI Bus Commands

PCI commands are encoded on the C/BE[3:0]# lines during the address phase:

| Command Code | Command Name              | Description                              |
|--------------|---------------------------|------------------------------------------|
| 0000         | Interrupt Acknowledge     | Acknowledge interrupt                    |
| 0001         | Special Cycle             | Message broadcast to all devices         |
| 0010         | I/O Read                  | Read data from I/O space                 |
| 0011         | I/O Write                 | Write data to I/O space                  |
| 0100         | Reserved                  | Reserved                                 |
| 0101         | Reserved                  | Reserved                                 |
| 0110         | Memory Read               | Read data from memory space              |
| 0111         | Memory Write              | Write data to memory space               |
| 1000         | Reserved                  | Reserved                                 |
| 1001         | Reserved                  | Reserved                                 |
| 1010         | Configuration Read        | Read configuration space                 |
| 1011         | Configuration Write       | Write to configuration space             |
| 1100         | Memory Read Multiple      | Burst read with multiple cache lines     |
| 1101         | Dual Address Cycle        | 64-bit addressing                        |
| 1110         | Memory Read Line          | Read an entire cache line                |
| 1111         | Memory Write and Invalidate | Write entire cache line and invalidate |

### 2.3 PCI Transaction Types

The PCI bus supports several transaction types:

**Single Data Phase Transactions:**
- Standard single-word read or write operations (I/O or memory)
- Typically used for register access in peripheral devices

**Burst Transactions:**
- Multiple data phase transfers within the same transaction
- Memory Read Line: For cache line fills
- Memory Read Multiple: For multiple cache line accesses
- Memory Write and Invalidate: For entire cache line writes

**Configuration Transactions:**
- Type 0: Direct access to local devices
- Type 1: Forwarded to another PCI bus segment (bridges)
- Used for automatic device configuration and resource assignment

**Special Cycle Transactions:**
- Broadcast messages to all devices on the PCI bus
- Used for system-wide notifications

### 2.4 PCI Bus Protocol

The PCI bus protocol is based on a master-slave model with the following key characteristics:

**Address/Data Phases:**
- Each transaction consists of an address phase followed by one or more data phases
- During the address phase, the master drives the address on AD[31:0] and command on C/BE[3:0]#
- During data phases, AD[31:0] carries the data and C/BE[3:0]# serves as byte enables

**Transaction Control:**
- FRAME# asserted during address phase to start a transaction
- IRDY# asserted by master when ready to transfer data
- TRDY# asserted by target when ready to transfer data
- Data transfer occurs when both IRDY# and TRDY# are asserted
- STOP# asserted by target to terminate a transaction

**Wait States:**
- Either master or target can insert wait states by deasserting IRDY# or TRDY#
- Allows devices to pace data transfer according to their capabilities

**Transaction Completion:**
- Normal completion: Master deasserts FRAME# during last data phase
- Disconnect: Target asserts STOP# to terminate transaction early
- Retry: Target asserts STOP# and doesn't assert TRDY#
- Target Abort: Target asserts STOP# and deasserts DEVSEL#
- Master Abort: No target claims the transaction (DEVSEL# not asserted)

### 2.5 PCI Configuration Space

Each PCI device implements a 256-byte configuration space with a standardized header:

| Offset | Field                         | Size    |
|--------|-------------------------------|---------|
| 00h    | Vendor ID                     | 16 bits |
| 02h    | Device ID                     | 16 bits |
| 04h    | Command Register              | 16 bits |
| 06h    | Status Register               | 16 bits |
| 08h    | Revision ID                   | 8 bits  |
| 09h    | Class Code                    | 24 bits |
| 0Ch    | Cache Line Size               | 8 bits  |
| 0Dh    | Latency Timer                 | 8 bits  |
| 0Eh    | Header Type                   | 8 bits  |
| 0Fh    | BIST                          | 8 bits  |
| 10h-27h| Base Address Registers (BARs) | 6 x 32 bits |
| 28h-2Fh| Reserved                      | 32 bits |
| 30h-33h| Expansion ROM Base Address    | 32 bits |
| 34h    | Capabilities Pointer          | 8 bits  |
| 35h-3Bh| Reserved                      | 28 bits |
| 3Ch    | Interrupt Line                | 8 bits  |
| 3Dh    | Interrupt Pin                 | 8 bits  |
| 3Eh    | Min_Gnt                       | 8 bits  |
| 3Fh    | Max_Lat                       | 8 bits  |

### 2.6 PCI Arbitration

PCI uses a central arbitration scheme:
- Each master has a unique REQ# output and GNT# input
- Masters assert REQ# to request bus ownership
- Arbiter asserts GNT# to grant bus ownership
- A master may continue using the bus until another device is granted control
- Hidden arbitration: arbitration overlaps with current transaction
- Standard requires fair access to the bus, preventing starvation

### 2.7 PCI Interrupt Handling

PCI provides four interrupt lines per slot:
- INTA#, INTB#, INTC#, and INTD# (active low)
- Interrupt lines are rotated between slots to spread the load
- Single-function devices typically use INTA#
- Multi-function devices may use multiple interrupt lines
- INTx routing to system interrupts is system-dependent
- PCI 2.2 later added Message Signaled Interrupts (MSI) capability

## 3. Building the PCI Protocol Analyzer DLL

### 3.1 Prerequisites

To build the PCI Protocol Analyzer DLL, you'll need:
- Microsoft Visual Studio 2015 or later
- Windows SDK (appropriate for your Visual Studio version)
- C++ development experience
- Administrative privileges for installation

### 3.2 Create a Visual Studio Project

1. Launch Visual Studio and select **File > New > Project**
2. Select **Visual C++ > Win32 > Win32 Project**
3. Name the project "PCI" and click **OK**
4. In the Win32 Application Wizard:
   - Click **Next**
   - Under Application Type, select **DLL**
   - Under Additional options, select **Empty project**
   - Click **Finish**

### 3.3 Add Source Files

1. Add the provided source files to the project:
   - Right-click on the project in Solution Explorer
   - Select **Add > Existing Item**
   - Browse to and select PCI.h, PCI.cpp, and PCI.def

### 3.4 Configure Project Settings

1. Right-click on the project and select **Properties**
2. Set the following configuration:
   - **Configuration**: Release
   - **Platform**: Win32 (x86)
   - **C/C++ > General > Additional Include Directories**: Add Windows SDK include path if needed
   - **Linker > Input > Module Definition File**: PCI.def
   - **Linker > System > SubSystem**: Windows (/SUBSYSTEM:WINDOWS)
   - **C/C++ > Code Generation > Runtime Library**: Multi-threaded DLL (/MD)

### 3.5 Build the DLL

1. Select **Build > Build Solution** (or press F7)
2. The DLL will be built in the Release folder of your project

## 4. Installation Guide

### 4.1 TLA700 System Requirements

- TLA700 series Logic Analyzer (TLA704, TLA714, TLA715, etc.)
- TLA Application Software version 4.1 or later
- Windows operating system (Windows 7 or later)
- Minimum of 136 acquisition channels for full PCI signal capture

### 4.2 Install the Protocol Analyzer

1. **Copy the DLL to the TLA Packages Directory**:
   - Locate the Release folder in your Visual Studio project
   - Copy the PCI.DLL file to: `C:\Program Files\TLA 700\Packages\`
   - If your TLA software is installed in a different location, adjust the path accordingly

2. **Copy the Setup File**:
   - Copy the PCI.tla file to: `C:\Program Files\TLA 700\Setups\`

3. **Register the Protocol Package**:
   - Open a Command Prompt with Administrator privileges
   - Navigate to the TLA installation directory:
     ```
     cd "C:\Program Files\TLA 700\"
     ```
   - Register the DLL using regsvr32:
     ```
     regsvr32 Packages\PCI.DLL
     ```
   - You should see a confirmation dialog indicating successful registration

4. **Verify Installation**:
   - Launch the TLA Application Software
   - Select **File > Open Setup**
   - Browse to the Setups directory
   - Verify that PCI.tla appears in the file list

## 5. Configuration and Setup

### 5.1 Connect the Logic Analyzer Probes

The PCI bus requires multiple channels to capture all signals. Connect the TLA probes to the PCI bus according to this mapping:

1. **Primary PCI Signal Group**:
   - Channel A0_0: PCI_CLK
   - Channel A0_1: PCI_RST#
   - Channel A0_2: PCI_FRAME#
   - Channel A0_3: PCI_IRDY#
   - Channel A0_4: PCI_TRDY#
   - Channel A0_5: PCI_STOP#
   - Channel A0_6: PCI_DEVSEL#
   - Channel A0_7: PCI_PAR
   - Channel A0_8: PCI_PERR#
   - Channel A0_9: PCI_SERR#
   - Channel A0_10: PCI_LOCK#

2. **Command/Byte Enable Signals**:
   - Channel A1_0: PCI_C_BE0#
   - Channel A1_1: PCI_C_BE1#
   - Channel A1_2: PCI_C_BE2#
   - Channel A1_3: PCI_C_BE3#

3. **Interrupt Signals**:
   - Channel A1_4: PCI_INTA#
   - Channel A1_5: PCI_INTB#
   - Channel A1_6: PCI_INTC#
   - Channel A1_7: PCI_INTD#

4. **Arbitration Signals**:
   - Channel A2_0: PCI_REQ0#
   - Channel A2_1: PCI_REQ1#
   - Channel A2_2: PCI_REQ2#
   - ...
   - Channel A2_7: PCI_GNT0#
   - Channel A3_0: PCI_GNT1#
   - Channel A3_1: PCI_GNT2#
   - ...

5. **Address/Data Signals (AD[0:31])**:
   - Channel A4_0 through A7_7: PCI_AD0 through PCI_AD31

6. **64-bit Extension Signals (Optional)**:
   - Channel A8_0 through A11_7: PCI_AD32 through PCI_AD63
   - Channel A12_0 through A12_3: PCI_C_BE4# through PCI_C_BE7#
   - Channel A12_4: PCI_PAR64

### 5.2 Physical Connection Methods

For connecting to a PCI bus, several options are available:

1. **PCI Bus Extender Card**:
   - Install a PCI extender card in the slot of interest
   - Connect the logic analyzer probes to test points on the extender card
   - Recommended for full signal fidelity and minimal interference

2. **PCI Interposer**:
   - Use a specialized PCI interposer that provides accessible test points
   - Connect the logic analyzer probes to the interposer
   - Best for production testing environments

3. **Direct Connection**:
   - For development boards with test points, connect directly
   - Use appropriate clips or probes suitable for your board
   - Requires careful management of probe leads

4. **Tektronix P67xx Series Probes**:
   - If available, use the Tektronix PCI-specific probes
   - These provide properly terminated connections for PCI signals
   - Designed specifically for the TLA700 series analyzers

### 5.3 Load the PCI Setup File

1. Launch the TLA Application
2. Select **File > Open Setup**
3. Browse to the Setups directory and select PCI.tla
4. Click **Open**

### 5.4 Configure Protocol Settings

1. In the TLA application, go to the Module menu
2. Select **PCI Protocol Settings**
3. Configure the following parameters:

   - **Bus Width**:
     - 32-bit: Standard PCI bus
     - 64-bit: Extended PCI bus
   
   - **Bus Speed**:
     - 33 MHz: Standard PCI clock rate
     - 66 MHz: High-speed PCI
   
   - **Arbitration Mode**:
     - Simple: Basic arbitration
     - Fairness: Round-robin arbitration
     - Priority: Priority-based arbitration
   
   - **PCI Signaling Voltage**:
     - 5V: 5-volt signaling environment
     - 3.3V: 3.3-volt signaling environment
     - Universal: Support for both voltage environments
   
   - **Latency**:
     - Minimal: Strict latency enforcement
     - Standard: Normal latency timing
     - Extended: Relaxed latency enforcement
   
   - **Error Detection Level**:
     - Basic: Detect only critical protocol errors
     - Standard: Detect common protocol violations
     - Advanced: Detect all protocol and timing violations

4. Click **OK** to save the settings

## 6. Using the PCI Protocol Analyzer

### 6.1 Setting Up a Basic Acquisition

1. In the TLA application, select **Setup > Trigger**
2. Configure a simple trigger condition:
   - For basic PCI activity: Trigger on FRAME# assertion
   ```
   FRAME# == 0
   ```
   - For specific transaction types: Use the C/BE lines during address phase
   ```
   (FRAME# == 0) AND (C_BE[3:0]# == 0110)  // Memory Read
   ```

3. Set the storage options:
   - Select **Setup > Storage**
   - Set an appropriate duration (e.g., 512K samples)
   - Configure the pre/post trigger ratio (typically 20% pre, 80% post)

4. Start the acquisition:
   - Click the **Run** button in the toolbar
   - The TLA will start acquisition and trigger according to your settings

### 6.2 Analyzing PCI Transactions

After acquisition completes, the TLA will display decoded PCI transactions:

1. **Transaction Display**:
   - Each decoded transaction shows the command type, address, data, and status
   - Example: "Memory Read 0x80000000 Data:0xABCD1234 BE:0xF"

2. **Multi-Data Phase Transactions**:
   - Burst transactions show the number of data phases
   - Example: "Memory Read Multiple 0x80000000 Burst:4 Data:0xABCD1234,0x12345678,..."

3. **Configuration Transactions**:
   - Shows device and function numbers
   - Example: "Configuration Read Dev:5 Func:0 Type0 Reg:0x04 Data:0x0107"

4. **Error Conditions**:
   - Displayed with a red background
   - Example: "Memory Write 0x90000000 [Master Abort]"

5. **Wait States**:
   - Shows the number of wait states inserted
   - Example: "I/O Read Port:0x3F8 Data:0x5A Wait:2"

6. **Arbitration Events**:
   - Shows bus ownership changes
   - Example: "Bus Grant: Master 3"

### 6.3 Transaction Details

Double-click on any transaction to see more details:

1. **Command Information**:
   - Full command name and code
   - Direction (read/write)
   - Transfer type (single, burst, etc.)

2. **Address Details**:
   - Full address (32-bit or 64-bit)
   - Address space (memory, I/O, configuration)
   - For configuration cycles: device, function, register

3. **Data Details**:
   - All data values transferred
   - Byte enables for each data phase
   - Calculated transfer size

4. **Timing Information**:
   - Transaction duration
   - Individual phase timings
   - Wait states inserted

5. **Protocol Signals**:
   - State of all PCI signals during the transaction
   - Timing diagram of critical signals
   - Protocol state transitions

### 6.4 Saving and Reporting

1. **Save the Acquisition**:
   - Select **File > Save As**
   - Choose a location and filename
   - Select file type TLA Data (.tld)

2. **Export Results**:
   - Select **File > Export**
   - Choose a format (CSV, TXT, etc.)
   - Specify which data to include

3. **Generate Reports**:
   - Select **Tools > Report Generator**
   - Choose a report template
   - Configure the report parameters
   - Generate a detailed PCI transaction report

## 7. Troubleshooting Common PCI Issues

### 7.1 Bus Arbitration Problems

**Symptoms**:
- Devices fail to gain bus access
- Long delays before transactions
- Uneven distribution of bus access

**Analysis Techniques**:
1. Trigger on REQ#/GNT# transitions
2. Measure the time between REQ# assertion and GNT# assertion
3. Check for proper deassertion of REQ# after transaction completion
4. Verify that no device holds GNT# for excessive periods
5. Check Latency Timer values in device configuration registers

### 7.2 Configuration Issues

**Symptoms**:
- Devices not recognized by the system
- Failed configuration reads/writes
- Initialization errors

**Analysis Techniques**:
1. Trigger on configuration cycles (C/BE[3:0]# = 1010 or 1011)
2. Verify proper IDSEL assertion for the target device
3. Check for timeouts or aborts during configuration
4. Examine the data returned during configuration reads
5. Check for proper Type 0/Type 1 configuration addressing

### 7.3 Data Transfer Problems

**Symptoms**:
- Data corruption
- Incomplete transfers
- Unexpected disconnects

**Analysis Techniques**:
1. Trigger on specific address ranges
2. Check parity signals (PAR, PERR#)
3. Verify proper byte enable patterns
4. Look for unexpected STOP# assertions
5. Monitor latency between phases
6. Check for proper DEVSEL# timing

### 7.4 Master/Target Aborts

**Symptoms**:
- Failed transactions
- System crashes
- Intermittent errors

**Analysis Techniques**:
1. Trigger on DEVSEL# timing anomalies
2. Look for transactions where DEVSEL# never asserts (master abort)
3. Check for premature DEVSEL# deassertion (target abort)
4. Verify proper address decoding by targets
5. Check status registers for abort indications

### 7.5 Timing Violations

**Symptoms**:
- Intermittent failures
- System instability
- Data corruption

**Analysis Techniques**:
1. Measure clock-to-signal timing
2. Verify setup and hold times for critical signals
3. Check for signal integrity issues (ringing, overshoot)
4. Examine turnaround cycles between transactions
5. Verify proper IRDY#/TRDY# timing relationships

### 7.6 Interrupt Issues

**Symptoms**:
- Missed interrupts
- Spurious interrupts
- System hangs

**Analysis Techniques**:
1. Trigger on INTx# assertion
2. Check for proper interrupt acknowledge cycles
3. Verify interrupt line routing
4. Examine device configuration space interrupt settings
5. Check for proper interrupt sharing behavior

## 8. Advanced Analysis Techniques

### 8.1 Performance Analysis

1. **Bandwidth Utilization**:
   - Measure the ratio of active bus cycles to total cycles
   - Calculate effective bandwidth: (data_bytes × clock_frequency) ÷ total_cycles
   - Identify efficiency bottlenecks

2. **Latency Analysis**:
   - Measure master latency: time from bus request to first data transfer
   - Measure target latency: time from address phase to first data transfer
   - Identify bottlenecks in the transaction flow

3. **Burst Efficiency**:
   - Calculate average data phases per transaction
   - Compare actual vs. theoretical maximum transfer rates
   - Identify premature transaction terminations

### 8.2 Correlating with Software Execution

1. **Software Trigger Markers**:
   - Use software to trigger specific PCI operations
   - Correlate PCI bus activity with program execution

2. **Driver Analysis**:
   - Trace driver-initiated PCI transactions
   - Identify inefficient access patterns
   - Detect redundant transactions

3. **DMA Performance**:
   - Measure DMA transfer rates
   - Analyze bus mastering efficiency
   - Identify contention issues

### 8.3 Multi-Master Analysis

1. **Arbitration Fairness**:
   - Track bus ownership over time
   - Measure time-to-grant for each master
   - Identify starvation or dominance issues

2. **Master Interactions**:
   - Analyze how multiple masters share the bus
   - Look for lock operations that may block other masters
   - Identify inefficient interleaving of transactions

### 8.4 Bridge Analysis

1. **PCI-to-PCI Bridge Transactions**:
   - Analyze Type 1 configuration cycles
   - Track forwarded transactions
   - Measure bridge latency and overhead

2. **Address Translations**:
   - Verify proper address forwarding rules
   - Check for proper prefetchable memory handling
   - Analyze posted write performance

### 8.5 Power Management Analysis

1. **Power State Transitions**:
   - Track device power state changes via configuration space
   - Measure transition times
   - Identify power state compatibility issues

2. **Clock Management**:
   - Analyze clock gating behavior
   - Check for improper transactions during power transitions
   - Verify proper power management event handling

## 9. References

1. PCI Local Bus Specification, Revision 2.3, PCI Special Interest Group, 2002.

2. TLA700 Series Logic Analyzer User Manual, Tektronix, Inc.

3. Shanley, Tom, and Don Anderson. "PCI System Architecture." Mindshare, Inc., 1999.

4. Solari, Edward, and George Willse. "PCI Hardware and Software: Architecture and Design." Annabooks, 1998.

5. Budruk, Ravi, Don Anderson, and Tom Shanley. "PCI Express System Architecture." Addison-Wesley Professional, 2003.


# PCI Protocol Analyzer for TLA700: Installation Guide

This document provides detailed instructions for installing and setting up the full 32/64-bit PCI Protocol Analyzer for the TLA700 Logic Analyzer.

## System Requirements

- Tektronix TLA700 Series Logic Analyzer
- TLA Application Software v4.4 or later
- At least 136 acquisition channels (for full signal coverage)
- Windows 2000 or higher.

## Installation Procedure

### 1. Copy Files

1. **Copy the DLL file**:
   - Locate the `PCI.dll` file in the installation package
   - Copy this file to `C:\Program Files\TLA 700\Packages\`
   - For 64-bit systems, the path may be `C:\Program Files (x86)\TLA 700\Packages\`

2. **Copy the setup file**:
   - Locate the `PCI.tla` file in the installation package
   - Copy this file to `C:\Program Files\TLA 700\Setups\`
   - For 64-bit systems, the path may be `C:\Program Files (x86)\TLA 700\Setups\`

### 2. Verify Installation

1. Close the TLA Application if it's currently running
2. Restart the TLA Application
3. In the TLA Application, click on "File" > "Open Setup"
4. Navigate to the location where you saved the PCI.tla file
5. Select the file and click "Open"
6. The TLA Application should load the PCI protocol analyzer setup without errors
7. If any error messages appear about missing modules or files, verify that the PCI.dll was properly copied to the Packages directory

### 3. Hardware Connection

For complete PCI bus analysis, you'll need to connect your logic analyzer probes to the PCI bus signals. This can be done using:

1. **Direct Connection**: Using logic analyzer probes directly on the PCI connector pins
2. **PCI Extender Card**: Using a PCI bus extender card with test points or headers
3. **Custom Adapter**: Using a custom-built adapter that connects between a PCI slot and the card being tested

#### Connection Diagram

For a typical setup with P6434 logic analyzer probes:

1. **Pod A0 (Control Signals 1)**:
   - Channel 0: CLK (PCI Clock)
   - Channel 1: RST# (Reset)
   - Channel 2: FRAME# (Cycle Frame)
   - Channel 3: IRDY# (Initiator Ready)
   - Channel 4: TRDY# (Target Ready)
   - Channel 5: STOP# (Stop Request)
   - Channel 6: DEVSEL# (Device Select)
   - Channel 7: LOCK# (Bus Lock)

2. **Pod A1 (Control Signals 2)**:
   - Channel 0: PERR# (Parity Error)
   - Channel 1: SERR# (System Error)
   - Channel 2: PAR (Parity)
   - Channel 3: IDSEL (Initialization Device Select)
   - Channel 4: INTA# (Interrupt A)
   - Channel 5: INTB# (Interrupt B)
   - Channel 6: INTC# (Interrupt C)
   - Channel 7: INTD# (Interrupt D)

3. **Pod A2 (Command/Byte Enable & Arbitration)**:
   - Channel 0: C/BE0# (Command/Byte Enable 0)
   - Channel 1: C/BE1# (Command/Byte Enable 1)
   - Channel 2: C/BE2# (Command/Byte Enable 2)
   - Channel 3: C/BE3# (Command/Byte Enable 3)
   - Channel 4: REQ# (Request)
   - Channel 5: GNT# (Grant)
   - Channel 6: SBO# (Snoop Backoff)
   - Channel 7: SDONE (Snoop Done)

4. **Pods A3-A6 (Address/Data Bus)**:
   - Pod A3, Channels 0-7: AD0-AD7 (Address/Data lines 0-7)
   - Pod A4, Channels 0-7: AD8-AD15 (Address/Data lines 8-15)
   - Pod A5, Channels 0-7: AD16-AD23 (Address/Data lines 16-23)
   - Pod A6, Channels 0-7: AD24-AD31 (Address/Data lines 24-31)

5. **Pods A7-A10 (64-bit Extension - Optional)**:
   - Pod A7, Channels 0-7: AD32-AD39 (Address/Data lines 32-39)
   - Pod A8, Channels 0-7: AD40-AD47 (Address/Data lines 40-47)
   - Pod A9, Channels 0-7: AD48-AD55 (Address/Data lines 48-55)
   - Pod A10, Channels 0-7: AD56-AD63 (Address/Data lines 56-63)
   - Pod A11, Channels 0-3: C/BE4#-C/BE7# (Command/Byte Enable 4-7)
   - Pod A11, Channel 4: PAR64 (64-bit Parity)

#### PCI Bus Connector Pinout Reference

Below is the standard PCI bus connector pinout for reference (32-bit section):

**Component Side (Side B)**:

| Pin | Signal    | Pin | Signal    |
|-----|-----------|-----|-----------|
| B1  | -12V      | B32 | +3.3V     |
| B2  | TCK       | B33 | +3.3V     |
| B3  | Ground    | B34 | Reserved  |
| B4  | TDO       | B35 | Ground    |
| B5  | +5V       | B36 | JTAG      |
| B6  | +5V       | B37 | Ground    |
| B7  | INTB#     | B38 | INTA#     |
| B8  | INTD#     | B39 | REQ#      |
| B9  | PRSNT1#   | B40 | PRSNT2#   |
| B10 | Reserved  | B41 | +5V       |
| B11 | PRSNT1#   | B42 | Ground    |
| B12 | Ground    | B43 | PAR       |
| B13 | Ground    | B44 | AD[15]    |
| B14 | Reserved  | B45 | AD[13]    |
| B15 | Ground    | B46 | AD[11]    |
| B16 | CLK       | B47 | AD[09]    |
| B17 | Ground    | B48 | C/BE#[0]  |
| B18 | REQ#      | B49 | Ground    |
| B19 | +5V       | B50 | Ground    |
| B20 | AD[31]    | B51 | AD[03]    |
| B21 | AD[29]    | B52 | AD[01]    |
| B22 | Ground    | B53 | +3.3V     |
| B23 | AD[27]    | B54 | AD[23]    |
| B24 | AD[25]    | B55 | Ground    |
| B25 | +3.3V     | B56 | DEVSEL#   |
| B26 | C/BE#[3]  | B57 | Ground    |
| B27 | AD[22]    | B58 | LOCK#     |
| B28 | AD[20]    | B59 | PERR#     |
| B29 | Ground    | B60 | +3.3V     |
| B30 | AD[18]    | B61 | SERR#     |
| B31 | AD[16]    | B62 | +3.3V     |

**Solder Side (Side A)**:

| Pin | Signal    | Pin | Signal    |
|-----|-----------|-----|-----------|
| A1  | TRDY#     | A32 | +3.3V     |
| A2  | +12V      | A33 | +3.3V     |
| A3  | TMS       | A34 | Reserved  |
| A4  | TDI       | A35 | Ground    |
| A5  | +5V       | A36 | JTAG      |
| A6  | +5V       | A37 | Ground    |
| A7  | INTC#     | A38 | RESET#    |
| A8  | +5V       | A39 | GNT#      |
| A9  | Reserved  | A40 | Reserved  |
| A10 | +I/O      | A41 | Reserved  |
| A11 | PRSNT2#   | A42 | Ground    |
| A12 | Ground    | A43 | PAR64     |
| A13 | Ground    | A44 | AD[14]    |
| A14 | Reserved  | A45 | AD[12]    |
| A15 | Ground    | A46 | AD[10]    |
| A16 | CLK       | A47 | AD[08]    |
| A17 | Ground    | A48 | AD[07]    |
| A18 | GNT#      | A49 | AD[05]    |
| A19 | Ground    | A50 | AD[04]    |
| A20 | Reserved  | A51 | AD[02]    |
| A21 | AD[30]    | A52 | AD[00]    |
| A22 | AD[28]    | A53 | +3.3V     |
| A23 | Ground    | A54 | AD[24]    |
| A24 | AD[26]    | A55 | Ground    |
| A25 | +3.3V     | A56 | FRAME#    |
| A26 | C/BE#[2]  | A57 | Ground    |
| A27 | AD[21]    | A58 | IRDY#     |
| A28 | AD[19]    | A59 | +3.3V     |
| A29 | +3.3V     | A60 | STOP#     |
| A30 | AD[17]    | A61 | REQ64#    |
| A31 | C/BE#[1]  | A62 | +5V       |

**64-bit PCI extension (Side B)**:

| Pin | Signal    | Pin | Signal    |
|-----|-----------|-----|-----------|
| B63 | Ground    | B94 | Ground    |
| B64 | C/BE#[4]  | B95 | C/BE#[7]  |
| B65 | AD[32]    | B96 | AD[63]    |
| B66 | AD[33]    | B97 | Ground    |
| B67 | Ground    | B98 | AD[61]    |
| B68 | AD[35]    | B99 | AD[59]    |
| B69 | AD[37]    | B100| Ground    |
| B70 | Ground    | B101| AD[57]    |
| B71 | AD[39]    | B102| AD[55]    |
| B72 | AD[41]    | B103| +3.3V     |
| B73 | +3.3V     | B104| C/BE#[6]  |
| B74 | AD[43]    | B105| AD[52]    |
| B75 | AD[45]    | B106| Ground    |
| B76 | Ground    | B107| AD[50]    |
| B77 | AD[47]    | B108| AD[48]    |
| B78 | AD[49]    | B109| Ground    |
| B79 | +5V       | B110| Reserved  |
| B80 | AD[51]    | B111| Reserved  |
| B81 | AD[53]    | B112| Ground    |
| B82 | Ground    | B113| Reserved  |
| B83 | AD[54]    | B114| Reserved  |
| B84 | C/BE#[5]  | B115| +3.3V     |
| B85 | +5V       | B116| PRSNT2#   |
| B86 | AD[56]    | B117| PRSNT2#   |
| B87 | AD[58]    | B118| Ground    |
| B88 | Ground    | B119| Reserved  |
| B89 | AD[60]    | B120| Reserved  |
| B90 | AD[62]    | B121| +3.3V     |
| B91 | Ground    | B122| Reserved  |
| B92 | Reserved  | B123| Ground    |
| B93 | Ground    | B124| Reserved  |

**64-bit PCI extension (Side A)**:

| Pin | Signal    | Pin | Signal    |
|-----|-----------|-----|-----------|
| A63 | Ground    | A94 | Ground    |
| A64 | C/BE#[4]  | A95 | C/BE#[7]  |
| A65 | AD[32]    | A96 | AD[63]    |
| A66 | AD[34]    | A97 | Ground    |
| A67 | Ground    | A98 | AD[61]    |
| A68 | AD[36]    | A99 | AD[59]    |
| A69 | AD[38]    | A100| Ground    |
| A70 | Ground    | A101| AD[57]    |
| A71 | AD[40]    | A102| AD[55]    |
| A72 | AD[42]    | A103| +3.3V     |
| A73 | +3.3V     | A104| C/BE#[6]  |
| A74 | AD[44]    | A105| AD[52]    |
| A75 | AD[46]    | A106| Ground    |
| A76 | Ground    | A107| AD[50]    |
| A77 | Reserved  | A108| AD[48]    |
| A78 | Reserved  | A109| Ground    |
| A79 | +5V       | A110| Reserved  |
| A80 | Reserved  | A111| Reserved  |
| A81 | Reserved  | A112| Ground    |
| A82 | Ground    | A113| Reserved  |
| A83 | Reserved  | A114| Reserved  |
| A84 | Reserved  | A115| +3.3V     |
| A85 | +5V       | A116| PRSNT1#   |
| A86 | Reserved  | A117| PRSNT1#   |
| A87 | Reserved  | A118| Ground    |
| A88 | Ground    | A119| Reserved  |
| A89 | Reserved  | A120| Reserved  |
| A90 | Reserved  | A121| +3.3V     |
| A91 | Ground    | A122| Reserved  |
| A92 | Reserved  | A123| Ground    |
| A93 | Ground    | A124| Reserved  |

### 4. Ground Connection

Proper grounding is essential for accurate signal capture:

1. Connect the logic analyzer's ground leads to the PC's chassis ground
2. Use multiple ground connections (at least one ground per pod)
3. Keep ground leads as short as possible to minimize noise
4. For PCI, which can operate at up to 66 MHz, proper grounding is critical

### 5. Probe Setup

For optimal signal quality:

1. Use high-quality logic analyzer probes rated for at least 100 MHz bandwidth
2. Keep probe leads as short as possible
3. Avoid routing probe cables near power supplies or other noise sources
4. Use probe hooks or micro-clips for secure connections
5. Consider using impedance-matched probes for critical timing signals (CLK, FRAME#, IRDY#, TRDY#)
6. For 66 MHz PCI, ensure your probes are rated for higher frequencies

### 6. Configure Threshold Voltages

Set appropriate threshold voltages in the TLA700 application:

1. Open the PCI protocol analyzer setup
2. Go to the "Setup" menu > "Thresholds"
3. Set thresholds to the appropriate PCI levels:
   - For 5V PCI slots: 1.5V (standard TTL threshold)
   - For 3.3V PCI slots: 1.65V (standard LVTTL threshold)
   - For Universal PCI analyzer: Configure based on the slot voltage

### 7. Verify Signal Integrity

Before starting analysis, verify signal integrity:

1. Set up a simple real-time display showing the CLK signal
2. Trigger the analyzer and verify the clock signal has clean edges and stable timing
3. Check critical control signals (FRAME#, IRDY#, TRDY#) for proper logic levels
4. Verify the address and data buses show transitions during bus activity
5. For 66 MHz PCI, pay special attention to signal quality and integrity

### 8. Update Clock Settings

Configure the clock settings to match your system:

1. In the TLA Application, go to "Setup" > "Clock"
2. Select "Asynchronous" sampling mode
3. Set the sample rate to at least 5x your PCI bus clock frequency:
   - For 33 MHz PCI bus: Use at least 165 MHz sample rate
   - For 66 MHz PCI bus: Use at least 330 MHz sample rate
4. For enhanced timing resolution, use the highest available sample rate

### 9. Trigger Setup

Configure triggers to capture relevant PCI bus activity:

1. Basic trigger on bus cycle:
   - Trigger on FRAME# going active (low)

2. Trigger on specific I/O port access:
   - Trigger on FRAME# going active
   - AND C/BE[3:0]# = 0010 (I/O Read) or 0011 (I/O Write)
   - AND Address bus = [specific port address]

3. Trigger on specific memory access:
   - Trigger on FRAME# going active
   - AND C/BE[3:0]# = 0110 (Memory Read) or 0111 (Memory Write)
   - AND Address bus = [specific memory address]

4. Trigger on configuration access:
   - Trigger on FRAME# going active
   - AND C/BE[3:0]# = 1010 (Configuration Read) or 1011 (Configuration Write)

5. Trigger on error conditions:
   - Trigger on PERR# or SERR# assertion
   - Trigger on Master Abort (FRAME# active without DEVSEL# assertion)
   - Trigger on Target Abort (STOP# and DEVSEL# both active)

### 10. Troubleshooting Installation Issues

If you encounter issues during installation:

1. **DLL Loading Errors**:
   - Verify the PCI.dll is in the correct directory
   - Check for any Windows security restrictions (right-click the DLL and unblock if necessary)
   - Verify the DLL is compatible with your TLA software version

2. **Missing Protocol Option**:
   - Make sure you've restarted the TLA Application after copying the files
   - Check the TLA Application log files for any error messages

3. **Signal Connection Issues**:
   - Use the "State" display to verify all signals are being properly acquired
   - Check probe connections and ground leads
   - Verify signal threshold settings match your hardware (3.3V or 5V PCI)

4. **Decoding Problems**:
   - Verify CLK signal integrity - it's critical for proper protocol decoding
   - Adjust timing parameters in the analyzer settings
   - Check for proper setup of control signals (FRAME#, IRDY#, TRDY#, etc.)

5. **64-bit PCI Issues**:
   - Verify REQ64# and ACK64# signals are properly connected
   - Configure the analyzer for 64-bit operation in settings

# Updates and Support

For updates or support:

1. Check for the latest version of the PCI Protocol Analyzer at:
   [DartFrogTek's GitHub](https://github.com/DartFrogTek/TLA-700-ISA-PCI-Protocols)

2. For technical support, contact:
   paraboliclabs@gmail.com
