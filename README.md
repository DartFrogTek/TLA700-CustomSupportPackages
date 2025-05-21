# TLA700 Custom Support Packages
ISA and PCI Custom Support Packages for TLA700 with TLA software version 4.4.
- 68 Channel ISA minimal bus package. `ISA_Minimal` folder.
- 136 Channel ISA full bus package. `ISA` folder.
- 136 Channel ISA full bus package. `PCI` folder.

No real testing has been done except that each compiles with Visual Studio 6 on Windows 10 and all packages load in TLA software.

The ISA_Minimal is the only one which has been loaded onto an actual TLA7L2 module. Yet to be tested with an actual ISA bus.

## Compiling
If you fancy yourself, you can compile with Visual Studio 6.
- Double click the `ISA.dsw` workspace in the `ISA` folder.
- It will have all three DLL projects loaded. Make whichever project you want active and compile.

## Loading
- I move all files from the compiled debug folder in whichever package you've compiled.
- However you can move just the DLL file. The .tla file needs to be moved as well.
- Place folder/files on the TLA within `C:\Program Files\TLA700\Supports\PROJECTNAME`
Example: `C:\Program Files\TLA700\Supports\ISA_Minimal` contains `ISA_Minimal.dll` and `ISA_Minimal.tla`
	
You should be able to load the package onto a TLA700 module with the Load Module option.

# ISA_Mictor38 and PCI_Mictor38 Interposer boards.
- ISA and PCI interposer boards that allow the use of P3464 probes on ISA and PCI bus.
- Yet to be finalized because I need to ensure the ISA and PCI full bus packages are in working order.
- KiCAD projects.
