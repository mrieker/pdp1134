
FPGA/ARM board that plugs into Unibus on PDP-11/34.

Provides functionality of the KY-11 console, 124KW memory,
RL-11 disks, DL-11 serial port, KW-11 line clock.

Has a GUI interface (ccode/z11gui) as well as a TCL command line
interface (ccode/z11ctrl) for scripting.

See howto.txt for more information.

Pics of board and gui screen are in boardpic.jpg and guittypic.png

It uses a ZTurn Zynq 7020 board for the FPGA/ARM processor,
memory, SDCARD, ethernet.  The ARM processor runs the
Raspberry PI OS, with a kernel specific to the Zynq.

The board might work on other PDP-11s, I haven't tested it
with anything else.  The only thing that makes it '34 specific
are the two halt signals.  Other than that, it is just another
Unibus compatible device.

