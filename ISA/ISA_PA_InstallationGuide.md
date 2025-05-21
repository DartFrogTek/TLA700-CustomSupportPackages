# ISA Protocol Analyzer Installation Guide

This document provides detailed instructions for installing and setting up the full 8/16-bit ISA Protocol Analyzer for the TLA700 Logic Analyzer.

## System Requirements

- Tektronix TLA700 Series Logic Analyzer
- TLA Application Software v4.4 or later
- At least 136 acquisition channels (for full signal coverage)
- Windows 2000 or higher.

## Installation Procedure

### 1. Copy Files

1. **Copy the DLL file**:
   - Locate the `ISA.dll` file in the installation package
   - Copy this file to `C:\Program Files\TLA 700\Packages\`
   - For 64-bit systems, the path may be `C:\Program Files (x86)\TLA 700\Packages\`

2. **Copy the setup file**:
   - Locate the `ISA.tla` file in the installation package
   - Copy this file to `C:\Program Files\TLA 700\Setups\`
   - For 64-bit systems, the path may be `C:\Program Files (x86)\TLA 700\Setups\`

### 2. Verify Installation

1. Close the TLA Application if it's currently running
2. Restart the TLA Application
3. In the TLA Application, click on "File" > "Open Setup"
4. Navigate to the location where you saved the ISA.tla file
5. Select the file and click "Open"
6. The TLA Application should load the ISA protocol analyzer setup without errors
7. If any error messages appear about missing modules or files, verify that the ISA.dll was properly copied to the Packages directory

### 3. Hardware Connection

For complete ISA bus analysis, you'll need to connect your logic analyzer probes to the ISA bus signals. This can be done using:

1. **Direct Connection**: Using logic analyzer probes directly on the ISA connector pins
2. **ISA Extender Card**: Using an ISA bus extender card with test points or headers
3. **Custom Adapter**: Using a custom-built adapter that connects between an ISA slot and the card being tested

#### Connection Diagram

For a typical setup with P6434 logic analyzer probes:

1. **Pod A0 (Control Signals 1)**:
   - Channel 0: BCLK (Bus Clock)
   - Channel 1: ALE (Address Latch Enable)
   - Channel 2: IOR# (I/O Read)
   - Channel 3: IOW# (I/O Write)
   - Channel 4: MEMR# (Memory Read)
   - Channel 5: MEMW# (Memory Write)
   - Channel 6: REFRESH# (Memory Refresh)
   - Channel 7: MASTER# (Bus Master)

2. **Pod A1 (Control Signals 2)**:
   - Channel 0: SBHE# (System Bus High Enable)
   - Channel 1: IOCHRDY (I/O Channel Ready)
   - Channel 2: AEN (Address Enable)
   - Channel 3: IOCHK# (I/O Channel Check)
   - Channel 4: RESET (System Reset)
   - Channel 5: OSC (14.31818 MHz Oscillator)

3. **Pods A2-A4 (Address Bus)**:
   - Pod A2, Channels 0-7: A0-A7 (Address lines 0-7)
   - Pod A3, Channels 0-7: A8-A15 (Address lines 8-15)
   - Pod A4, Channels 0-7: A16-A23 (Address lines 16-23)

4. **Pods A5-A6 (Data Bus)**:
   - Pod A5, Channels 0-7: D0-D7 (Data lines 0-7)
   - Pod A6, Channels 0-7: D8-D15 (Data lines 8-15)

5. **Pods A7-A8 (DMA Signals)**:
   - Pod A7, Channels 0-6: DACK0#-DACK7# (DMA Acknowledge)
   - Pod A7, Channel 7 & Pod A8, Channels 0-5: DRQ0-DRQ7 (DMA Request)
   - Pod A8, Channel 6: TC (Terminal Count)

6. **Pods A9-A10 (Interrupt Signals)**:
   - Channels 0-11: IRQ2-IRQ15 (Interrupt Request lines)

#### ISA Bus Connector Pinout Reference

Below is the standard ISA bus connector pinout for reference (8-bit section):

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| A1  | IOCHK# | B1  | GND    |
| A2  | D7     | B2  | RESET  |
| A3  | D6     | B3  | +5V    |
| A4  | D5     | B4  | IRQ2   |
| A5  | D4     | B5  | -5V    |
| A6  | D3     | B6  | DRQ2   |
| A7  | D2     | B7  | -12V   |
| A8  | D1     | B8  | NOWS#  |
| A9  | D0     | B9  | +12V   |
| A10 | IOCHRDY| B10 | GND    |
| A11 | AEN    | B11 | SMEMW# |
| A12 | A19    | B12 | SMEMR# |
| A13 | A18    | B13 | IOW#   |
| A14 | A17    | B14 | IOR#   |
| A15 | A16    | B15 | DACK3# |
| A16 | A15    | B16 | DRQ3   |
| A17 | A14    | B17 | DACK1# |
| A18 | A13    | B18 | DRQ1   |
| A19 | A12    | B19 | REFRESH#|
| A20 | A11    | B20 | BCLK   |
| A21 | A10    | B21 | IRQ7   |
| A22 | A9     | B22 | IRQ6   |
| A23 | A8     | B23 | IRQ5   |
| A24 | A7     | B24 | IRQ4   |
| A25 | A6     | B25 | IRQ3   |
| A26 | A5     | B26 | DACK2# |
| A27 | A4     | B27 | TC     |
| A28 | A3     | B28 | ALE    |
| A29 | A2     | B29 | +5V    |
| A30 | A1     | B30 | OSC    |
| A31 | A0     | B31 | GND    |

16-bit ISA extension (additional):

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| C1  | SBHE#  | D1  | MEM CS16#|
| C2  | LA23   | D2  | I/O CS16#|
| C3  | LA22   | D3  | IRQ10  |
| C4  | LA21   | D4  | IRQ11  |
| C5  | LA20   | D5  | IRQ12  |
| C6  | LA19   | D6  | IRQ15  |
| C7  | LA18   | D7  | IRQ14  |
| C8  | LA17   | D8  | DACK0# |
| C9  | MEMR#  | D9  | DRQ0   |
| C10 | MEMW#  | D10 | DACK5# |
| C11 | D8     | D11 | DRQ5   |
| C12 | D9     | D12 | DACK6# |
| C13 | D10    | D13 | DRQ6   |
| C14 | D11    | D14 | DACK7# |
| C15 | D12    | D15 | DRQ7   |
| C16 | D13    | D16 | +5V    |
| C17 | D14    | D17 | MASTER#|
| C18 | D15    | D18 | GND    |

### 4. Ground Connection

Proper grounding is essential for accurate signal capture:

1. Connect the logic analyzer's ground leads to the PC's chassis ground
2. Use multiple ground connections (at least one ground per pod)
3. Keep ground leads as short as possible to minimize noise

### 5. Probe Setup

For optimal signal quality:

1. Use high-quality logic analyzer probes rated for at least 25 MHz bandwidth
2. Keep probe leads as short as possible
3. Avoid routing probe cables near power supplies or other noise sources
4. Use probe hooks or micro-clips for secure connections
5. Consider using impedance-matched probes for critical timing signals (BCLK, ALE)

### 6. Configure Threshold Voltages

Set appropriate threshold voltages in the TLA700 application:

1. Open the ISA protocol analyzer setup
2. Go to the "Setup" menu > "Thresholds"
3. Set thresholds to the appropriate TTL levels:
   - Recommended setting: 1.5V (standard TTL threshold)
   - For noisy environments: 1.4V for better noise immunity
   - For 3.3V ISA signals (if applicable): 1.65V

### 7. Verify Signal Integrity

Before starting analysis, verify signal integrity:

1. Set up a simple real-time display showing the BCLK signal
2. Trigger the analyzer and verify the clock signal has clean edges and stable timing
3. Check critical control signals (IOR#, IOW#, MEMR#, MEMW#) for proper logic levels
4. Verify the address and data buses show transitions during bus activity

### 8. Update Clock Settings

Configure the clock settings to match your system:

1. In the TLA Application, go to "Setup" > "Clock"
2. Select "Asynchronous" sampling mode
3. Set the sample rate to at least 5x your ISA bus clock frequency:
   - For 8 MHz ISA bus: Use at least 40 MHz sample rate
   - For 12 MHz ISA bus: Use at least 60 MHz sample rate
4. For enhanced timing resolution, use the highest available sample rate

### 9. Trigger Setup

Configure triggers to capture relevant ISA bus activity:

1. Basic trigger on bus cycle:
   - Trigger on IOR# OR IOW# OR MEMR# OR MEMW# going active (low)

2. Trigger on specific I/O port access:
   - Trigger on IOR# OR IOW# going active
   - AND Address bus = [specific port address]

3. Trigger on specific memory access:
   - Trigger on MEMR# OR MEMW# going active
   - AND Address bus = [specific memory address]

4. Trigger on interrupt:
   - Trigger on specific IRQ line going active (high)

5. Trigger on DMA activity:
   - Trigger on specific DACK# line going active (low)

### 10. Troubleshooting Installation Issues

If you encounter issues during installation:

1. **DLL Loading Errors**:
   - Verify the ISA.dll is in the correct directory
   - Check for any Windows security restrictions (right-click the DLL and unblock if necessary)
   - Verify the DLL is compatible with your TLA software version

2. **Missing Protocol Option**:
   - Make sure you've restarted the TLA Application after copying the files
   - Check the TLA Application log files for any error messages

3. **Signal Connection Issues**:
   - Use the "State" display to verify all signals are being properly acquired
   - Check probe connections and ground leads
   - Verify signal threshold settings match your hardware

4. **Decoding Problems**:
   - Verify BCLK signal integrity - it's critical for proper protocol decoding
   - Adjust timing parameters in the analyzer settings
   - Check for proper setup of control signals

# Comprehensive User Guide

## Full 8/16-bit ISA Protocol Analyzer for TLA700 Logic Analyzers

This package provides comprehensive support for the Industry Standard Architecture (ISA) bus protocol analysis with the Tektronix TLA700 series Logic Analyzers. It supports complete 8-bit and 16-bit ISA bus operation, including all control signals, address decoding, data transfers, DMA, and interrupts. The analyzer supports the complete ISA bus protocol with all signals, timing requirements, and transaction types, including 8-bit and 16-bit transfers, DMA operations, interrupts, and memory refresh cycles. The state machine implementation properly tracks the ISA bus protocol states (T1, T2, TW, T3) and detects protocol violations.

## Features

- **Complete 8/16-bit ISA Bus Support**: Analyzes all ISA bus signals according to the ISA specification
- **Comprehensive Signal Coverage**:
  - All control signals (IOR#, IOW#, MEMR#, MEMW#, ALE, SBHE#, etc.)
  - Full address bus (16/20/24-bit addressing)
  - 8-bit and 16-bit data transfers
  - DMA channels (0-3, 5-7)
  - Interrupt lines (IRQ2-7, 9-15)
  - REFRESH# memory refresh cycles
  - Bus master operation
- **Advanced Protocol Decoding**:
  - Accurate state machine implementation with T1, T2, TW, T3 states
  - Wait state detection and counting
  - Bus timing analysis
  - Protocol error detection
- **Customizable Settings**:
  - Address width (16/20/24-bit)
  - Bus speed (4.77/6/8/10/12 MHz)
  - DMA and refresh cycle support
  - Timing modes
  - Error detection level

## Installation
 - TODO: Create Installer package. 

1. Copy the ISA.DLL file to the TLA700 support package directory:
   ```
   C:\Program Files\TLA 700\Packages\
   ```

2. Copy the ISA.tla setup file to the TLA700 setups directory:
   ```
   C:\Program Files\TLA 700\Setups\
   ```

3. Restart the TLA application if it's currently running.

## Hardware Setup

Connect your logic analyzer probes to the ISA bus signals as follows:

### Control Signals (Pod A0, A1)
- A0_0: BCLK (Bus Clock)
- A0_1: ALE (Address Latch Enable)
- A0_2: IOR# (I/O Read, active low)
- A0_3: IOW# (I/O Write, active low)
- A0_4: MEMR# (Memory Read, active low)
- A0_5: MEMW# (Memory Write, active low)
- A0_6: REFRESH# (Memory Refresh, active low)
- A0_7: MASTER# (Bus Master, active low)
- A1_0: SBHE# (System Bus High Enable, active low)
- A1_1: IOCHRDY (I/O Channel Ready, active high)
- A1_2: AEN (Address Enable, active high)
- A1_3: IOCHK# (I/O Channel Check, active low)
- A1_4: RESET (System Reset, active high)
- A1_5: OSC (14.31818 MHz Oscillator)

### Address Bus (Pod A2, A3, A4)
- A2_0 - A2_7: A0-A7 (Address lines 0-7)
- A3_0 - A3_7: A8-A15 (Address lines 8-15)
- A4_0 - A4_7: A16-A23 (Address lines 16-23)

### Data Bus (Pod A5, A6)
- A5_0 - A5_7: D0-D7 (Data lines 0-7)
- A6_0 - A6_7: D8-D15 (Data lines 8-15)

### DMA Signals (Pod A7, A8)
- A7_0 - A7_6: DACK0#-DACK7# (DMA Acknowledge, active low)
- A7_7, A8_0 - A8_5: DRQ0-DRQ7 (DMA Request, active high)
- A8_6: TC (Terminal Count, active high)

### Interrupt Signals (Pod A9, A10)
- A9_0 - A9_7, A10_0 - A10_3: IRQ2-IRQ15 (Interrupt Request lines, active high)

## Using the Analyzer

1. Launch the TLA700 application.

2. Open the ISA.tla setup file:
   - Click on File > Open Setup
   - Navigate to the ISA.tla file and open it

3. Configure the analyzer settings:
   
   ### Address Width
   - **16-bit**: Standard ISA addressing (64KB)
   - **20-bit**: Extended ISA addressing (1MB)
   - **24-bit**: Enhanced ISA addressing (16MB)

   ### Bus Speed
   - **4.77 MHz**: Original PC/XT bus clock
   - **6 MHz**: Enhanced bus clock
   - **8 MHz**: AT bus clock
   - **10 MHz**: Fast AT bus clock
   - **12 MHz**: High-speed AT bus clock

   ### DMA Support
   - **Enabled**: Decode DMA transactions
   - **Disabled**: Ignore DMA transactions

   ### Refresh Support
   - **Enabled**: Decode memory refresh cycles
   - **Disabled**: Ignore refresh cycles

   ### Timing Mode
   - **Fast Mode**: Less strict timing verification
   - **Normal Mode**: Standard timing verification
   - **Slow Mode**: Relaxed timing verification
   - **AT Mode**: AT-specific timing
   - **ISA Mode**: Full ISA timing verification

   ### Error Detection
   - **Basic**: Only detect critical errors
   - **Advanced**: Detect all protocol violations and timing errors

4. Start acquisition:
   - Click on the Run button or press F5

5. Analyze the captured data:
   - The analyzer will display decoded ISA bus transactions with the following information:
     - Transaction type (I/O Read/Write, Memory Read/Write, DMA, Refresh)
     - Address (in hex, formatted according to address width)
     - Data value (8-bit or 16-bit)
     - Wait states count
     - For DMA: Channel number and Terminal Count status
     - For errors: Protocol violation details

## Trigger Setup

You can set up custom triggers to capture specific ISA bus events:

### Common Trigger Examples

1. **I/O Port Access**:
   - Set condition on ISA_IOR# or ISA_IOW# being active (low)
   - Add address condition for specific I/O port

2. **Memory Access to Specific Region**:
   - Set condition on ISA_MEMR# or ISA_MEMW# being active (low)
   - Add address condition for memory region of interest

3. **DMA Transfer**:
   - Set condition on specific DACK# signal being active (low)

4. **Interrupt Activity**:
   - Set condition on specific IRQ line being active (high)

5. **Bus Error**:
   - Set condition on ISA_IOCHK# being active (low)

## Understanding the Protocol Display

The analyzer displays ISA bus transactions with the following color coding:

- **White Background**: Normal I/O or memory transactions
- **Yellow Background**: DMA transactions
- **Grey Background**: Memory refresh cycles
- **Red Background**: Error conditions and protocol violations

Each transaction shows:
- Transaction type (e.g., "I/O Read (8-bit)")
- Address in hexadecimal (e.g., "Addr: 0x03F8")
- Data value in hexadecimal (e.g., "Data: 0x2F")
- Wait states count (e.g., "Wait: 2")
- Additional information for special transactions

## Troubleshooting

### No Transactions Displayed
- Verify all signals are properly connected to the correct channels
- Check that the BCLK signal has good signal quality
- Ensure address/data lines are properly connected

### Excessive Error Indications
- Try adjusting the Timing Mode to match your system
- Verify signal integrity, especially for BCLK and control signals
- Check for proper ground connections to the logic analyzer

### Missing 16-bit Transfers
- Verify the SBHE# signal is properly connected
- Check that all data lines D0-D15 are connected

### DMA or Interrupt Issues
- Verify proper connections for DACK#/DRQ or IRQ lines
- Make sure DMA Support or required IRQ lines are enabled in settings


# Updates and Support

For updates or support:

1. Check for the latest version of the ISA Protocol Analyzer at:
   [\[DFT's GitHub\]](https://github.com/DartFrogTek/TLA-700-ISA-PCI-Protocols)

2. For technical support, contact:
   paraboliclabs@gmail.com