
FPGA/ARM board that plugs into Unibus on PDP-11/34.

Provides functionality of the KY-11 console, 124KW memory,
RL-11 disks (RL01/RL02), DL-11 serial port, KW-11 line clock,
RH-11 disks (RP04/RP06), TM-11 tape drive, DELUA ethernet,
PC-11 paper tape reader/punch, DZ-11 serial line mux.

Also contains a PDP-11/34 processor simulator so can test
software without having real processor.

Has a GUI interface (ccode/z11gui) as well as a TCL command line
interface (ccode/z11ctrl) for scripting.

See howto.txt for more information.

Pics of board and gui screen are in boardpic.jpg and guittypic.png,
runningpic.jpg is pic of 10.5" screen mounted on front of PDP-11/34
with Zynq in place of console board.

It uses a ZTurn Zynq 7020 board for the FPGA/ARM processor,
memory, SDCARD, ethernet.  The ARM processor runs the
Raspberry PI OS, with a kernel specific to the Zynq.

The board might work on other PDP-11s, I haven't tested it
with anything else.  The only thing that makes it '34 specific
are the two halt signals.  Other than that, it is just another
Unibus compatible device, fitting in a quad-sized SPC slot.

