
# create RSX-11M V4.5 disk set

# start with:
#  /mdsk/mrieker/rsx40/rsxm32.rl01
#  /mdsk/mrieker/rsxtapes/rsx11m-45-BB-L974F-BC-bruhdr.tap
#  /mdsk/mrieker/rsxtapes/decnet-netkit-45-BB-M461E-BC.tap
#  /mdsk/mrieker/rsxtapes/decnet-deckit-45-BB-J050G-BC.tap

# outputs:
#  /mdsk/mrieker/rsxdisks/rsx45-dlsys.rl02
#  /mdsk/mrieker/rsxdisks/rsx45-excprv.rl02
#  /mdsk/mrieker/rsxdisks/rsx45-prvbld.rl02
#  /mdsk/mrieker/rsxdisks/rsx45-mcrsrc.rl02
#  /mdsk/mrieker/rsxdisks/rsx45-rmsv20.rl02
#  /mdsk/mrieker/rsxdisks/rsx45-netgen.rl02

# https://pdp2011.sytse.net/wordpress/pdp-11/sessions/rsx-11m-plus/install-5/
# https://supratim-sanyal.blogspot.com/2019/07/installing-rsx-11m-plus-on-dec-pdp-1124.html
# https://www.9track.net/pdp11/decnet4_netgen
# https://shop-pdp.net/rthtml/tcpget.php
# https://ftp.trailing-edge.com/pub/rsxdists/

source boots.tcl

if {[pin get fpgamode] == 0} {
    pin set fpgamode 1
}
hardreset
rlunload 0
rlunload 1
rlunload 2
rlunload 3
tmunload 0
tmunload 1
exec rm -rf /mdsk/mrieker/rsxdisks
exec mkdir -p /mdsk/mrieker/rsxdisks

if false {

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "MAKE BOOTABLE V4.5 SYSTEM DISK"
puts ""

probedevsandmem
rlload 0 /mdsk/mrieker/rsx40/rsxm32.rl01
rlload 1 -create /mdsk/mrieker/rsxdisks/rsx45-dlsys.rl02
tmload 0 -readonly /mdsk/mrieker/rsxtapes/rsx11m-45-BB-L974F-BC-bruhdr.tap
rlboot

replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
replytoprompt ">" "MOU MT0:/FOR"
replytoprompt ">" "MOU DL1:/FOR"
replytoprompt ">" "INS \$BRU"
replytoprompt ">" "BRU /REW/INI/BAC:DLSYS MT0: DL1:"
replytoprompt ">" "DMO MT0:"
replytoprompt ">" "DMO DL1:"
waitforcrlf

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "COPYING FROM TAPE TO DISKS"
puts ""

hardreset
rlunload 0
rlunload 1
tmunload 0

rlload 0 /mdsk/mrieker/rsxdisks/rsx45-dlsys.rl02
tmload 0 -readonly /mdsk/mrieker/rsxtapes/rsx11m-45-BB-L974F-BC-bruhdr.tap
rlboot

replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
replytoprompt ">" "LOA MT:"
replytoprompt ">" "@TAPEKIT"
replytoprompt "Enter disk type, RL01 or RL02 \[D:RL02\] \[S\]: " ""
replytoprompt "Enter second drive and unit \[ddn:\] \[S\]: " "DL1:"
replytoprompt "Enter drive and unit with tape distribution kit \[ddn:\] \[S\]: " "MT0:"

waitforstring "Load disk labeled EXCPRV in DL1:"
rlload 1 -create /mdsk/mrieker/rsxdisks/rsx45-excprv.rl02
replytoprompt "Is DL1: ready? \[Y/N\]: " "Y"

waitforstring "Load disk labeled PRVBLD in DL1:"
rlload 1 -create /mdsk/mrieker/rsxdisks/rsx45-prvbld.rl02
replytoprompt "Is DL1: ready? \[Y/N\]: " "Y"

waitforstring "Load disk labeled MCRSRC in DL1:"
rlload 1 -create /mdsk/mrieker/rsxdisks/rsx45-mcrsrc.rl02
replytoprompt "Is DL1: ready? \[Y/N\]: " "Y"

waitforstring "Load disk labeled RMSV20 in DL1:"
rlload 1 -create /mdsk/mrieker/rsxdisks/rsx45-rmsv20.rl02
replytoprompt "Is DL1: ready? \[Y/N\]: " "Y"
waitforstring ">@ <EOF>"
waitforcrlf

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "RUNNING SYSGEN"
puts ""

hardreset
rlunload 0
rlunload 1
tmunload 0

rlload 0 /mdsk/mrieker/rsxdisks/rsx45-dlsys.rl02
rlload 1 /mdsk/mrieker/rsxdisks/rsx45-excprv.rl02
rlload 2 /mdsk/mrieker/rsxdisks/rsx45-prvbld.rl02
rlboot

replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
replytoprompt ">" "@SYSGEN"

replytoprompt "Autoconfigure the host system hardware? \[Y/N\]: " "Y"
replytoprompt "Do you want to override Autoconfigure results? \[Y/N\]: " "N"
replytoprompt "Do you want to inhibit execution of MCR commands (PREPGEN)? \[Y/N\]: " "N"
replytoprompt "Have you made a copy of the distribution kit? \[Y/N\]: " "Y"
replytoprompt "Are you generating an unmapped system? \[Y/N\]: " "N"
replytoprompt "Use an input saved answer file? \[Y/N\]: " "N"
replytoprompt "Do you want a Standard Function System? \[Y/N\]: " "Y"
replytoprompt "Name of output saved answer file \[D: SYSSAVED.CMD\] \[S\]: " ""
replytoprompt "Clean up files from previous GENs? \[Y/N\]: " "Y"
replytoprompt "Chain to Phase II after Phase I completes? \[Y/N\]: " "Y"
replytoprompt "Enter device for EXCPRV disk when it is ready (ddu:) \[D: DL1:\] \[S\]: " ""
replytoprompt "Enter device to be used for PRVBLD disk (ddu:) \[D: DL2:\] \[S\]: " ""
replytoprompt "Line frequency:   A- 60 Hz    B- 50 Hz   \[D: A\] \[S\]: " ""
replytoprompt "Highest interrupt vector \[O R:0-774 D:0\]: " ""
replytoprompt "Devices \[S\]: " "."

replytoprompt "Is a line printer available? \[Y/N\]: " "N"
replytoprompt "Does the listing/map device have at least 120 columns? \[Y/N\]: " "Y"
replytoprompt "Assembly listings device (ddu:) \[D: \"NL:\"\] \[S\]: " ""
replytoprompt "Map device for Executive and device drivers (ddu:) \[D: DL2:\] \[S\]: " ""
replytoprompt "Executive Debugging Tool (XDT)? \[Y/N\]: " "N"
replytoprompt "Include support for communications products (such as DECnet)? \[Y/N\]: " "Y"
replytoprompt "Include Network Command Terminal support? \[Y/N\]: " "Y"
replytoprompt "Enter CDA memory dump device mnemonic (ddu:) \[S R:3-4\]: " "DL3:"
replytoprompt "Enter CDA memory dump device CSR \[O R:160000-177700 D:174400\]: " ""
replytoprompt "RT-11 emulation support? \[Y/N\]: " "N"
replytoprompt "Include support for the IP11 Industrial I/O Subsystem? \[Y/N\]: " "N"
replytoprompt "What name would you like to give your system \[D: RSX11M\] \[S R:0-6\]: " ""
replytoprompt "Do you want SPM-11 support? \[Y/N\]: " "N"

replytoprompt "MT controller 0 \[D: 224,172522\]              \[S\]: " "224,172522,2"
replytoprompt "YZ controller 0 \[D: ,,,0\]                    \[S\]: " "300,160100,7"
waitforstring "End of SYSGEN phase II at"
waitforstring ">@ <EOF>"
replytoprompt ">" "BOO \[1,54\]RSX11M"
waitforstring "RSX11M V4.5 BL50"
replytoprompt ">" ""
replytoprompt ">" "TIM [rsxdatetime]"
replytoprompt ">" "SAV /WB"
waitforstring "RSX-11M V4.5 BL50"
replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
waitforcrlf

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "SYSGEN PHASE III"
puts ""

# phase iii
# sysgen guide 6-1,7-1
replytoprompt ">" "INS \$PIP"
replytoprompt ">" "PIP \[*,*\]*.*/PU"
replytoprompt ">" "SET /UIC=\[200,200\]"
replytoprompt ">" "@SYSGEN3"

replytoprompt "In what UIC is SGNPARM.CMD if not in \[200,200\] \[S\]: " ""
replytoprompt "Are you building nonprivileged tasks? \[Y/N\]: " "Y"
replytoprompt "Is this an RL01 kit? \[Y/N\]: " "N"
replytoprompt "Enter device for PRVBLD device when it is ready (ddu:) \[D: DL1:\] \[S\]: " "DL2:"
replytoprompt "Enter map device (ddu:) \[D: NL:\] \[S\]: " ""
replytoprompt "Enter task name(s) \[S\]: " "%"
replytoprompt "Use \[1,1\]FCSRES.STB when building those tasks? \[Y/N\]: " "Y"
replytoprompt "Pause to edit any task build .CMD or .ODL files? \[Y/N\]: " "N"
replytoprompt "Delete task build .CMD and .ODL files after task building? \[Y/N\]: " "Y"
waitforstring ">@ <EOF>"

replytoprompt ">" "SET /UIC=\[1,54\]"
replytoprompt ">" "PIP \[*,*\]*.*/PU"
replytoprompt ">" "PIP ACF.BSL;1/DE"
replytoprompt ">" "PIP DPDRV.*;1/DE"
replytoprompt ">" "PIP FCPSML.TSK;1/DE"
replytoprompt ">" ""

## /mdsk/mrieker/rsxdisks-cp00.tgz

} else {
    exec rm -rf /mdsk/mrieker/rsxdisks
    exec tar xzvfC /mdsk/mrieker/rsxdisks-cp00.tgz /mdsk/mrieker < /dev/null > /dev/tty

    probedevsandmem
    rlload 0 /mdsk/mrieker/rsxdisks/rsx45-dlsys.rl02
    rlboot

    replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
    replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
    waitforstring ">@ <EOF>"
}

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "CREATE DECNET PARTITION (CEXPAR)"
puts ""

# see DL0:[5,24]CEXBLD.CMD PAR=CEXPAR:113500:4300 for magic numbers used below

replytoprompt ">" "SET /UIC=\[1,54\]"
replytoprompt ">" "INS \$EDT"
replytoprompt ">" "INS \$PIP"
replytoprompt ">" "INS \$VMR"
replytoprompt ">" "EDT SYSVMR.CMD"
replytoprompt "\n*" "'/POOL="
replytoprompt "\n*" "S/*/1135"
replytoprompt "\n*" ""
replytoprompt "\n*" "I;SET /MAIN=CEXPAR:*:43:COM"
replytoprompt "\n*" "EX"
replytoprompt ">" "PIP RSX11M.SYS/CO/BL:498.=RSX11M.TSK"
replytoprompt ">" "VMR @SYSVMR"
replytoprompt ">" "BOO \[1,54\]RSX11M"
waitforstring "RSX11M V4.5 BL50"
replytoprompt ">" ""
replytoprompt ">" "TIM [rsxdatetime]"
replytoprompt ">" "SAV /WB"
waitforstring "RSX-11M V4.5 BL50"
replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
waitforcrlf

puts "= = = = = = = = = = = = = = = ="
puts "tar czvfC /mdsk/mrieker/rsxdisks-cp10.tgz /mdsk/mrieker rsxdisks"
exec tar czvfC /mdsk/mrieker/rsxdisks-cp10.tgz /mdsk/mrieker rsxdisks < /dev/null > /dev/tty
puts "= = = = = = = = = = = = = = = ="

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "RUNNING DECNET PREGEN"
puts ""

tmload 0 -readonly /mdsk/mrieker/rsxtapes/decnet-netkit-45-BB-M461E-BC.tap

replytoprompt ">" "INS \$FLX"
replytoprompt ">" "UFD DL0:\[137,10\]"
replytoprompt ">" "SET /UIC=\[137,10\]"
replytoprompt ">" "FLX DL0:/UI=MT0:\[137,10\]PREGEN.CMD/RW"
replytoprompt ">" "ALL DL1:"
replytoprompt ">" "@DL0:PREGEN"

replytoprompt "Do you wish to see the PREGEN notes? \[Y/N\]: " "N"
replytoprompt "Are you running on a small dual-disk system? \[Y/N\]: " "N"
replytoprompt "Where is the Network distribution kit loaded \[S\]: " "MT0:"
rlload 1 -create /mdsk/mrieker/rsxdisks/rsx45-netgen.rl02
replytoprompt "Is the tape already loaded in MT0:? \[Y/N\]: " "Y"
replytoprompt "Is the tape 1600 BPI? \[Y/N\]: " "N"
replytoprompt "Where is the NETGEN disk loaded \[S\]: " "DL1:"
replytoprompt "Is the disk already loaded in DL1:? \[Y/N\]: " "Y"
replytoprompt "Should the NETGEN Disk be initialized? \[Y/N\]: " "Y"
replytoprompt "Should the Network object files be moved to the NETGEN Disk? \[Y/N\]: " "Y"

replytoprompt "Copy the DECnet distribution kit? \[Y/N\]: " "Y"
replytoprompt "Where is the DECnet distribution kit loaded \[S\]: " "MT0:"
tmload 0 -readonly /mdsk/mrieker/rsxtapes/decnet-deckit-45-BB-J050G-BC.tap
replytoprompt "Is the tape already loaded in MT0:? \[Y/N\]: " "Y"
replytoprompt "Is the tape 1600 BPI? \[Y/N\]: " "N"
replytoprompt "Should the DECnet object files be moved to the NETGEN Disk? \[Y/N\]: " "Y"
replytoprompt "Copy the PSI distribution kit? \[Y/N\]: " "N"
waitforstring ">@ <EOF>"
waitforcrlf

tmunload 0

puts "= = = = = = = = = = = = = = = ="
puts "tar czvfC /mdsk/mrieker/rsxdisks-cp11.tgz /mdsk/mrieker rsxdisks"
exec tar czvfC /mdsk/mrieker/rsxdisks-cp11.tgz /mdsk/mrieker rsxdisks < /dev/null > /dev/tty
puts "= = = = = = = = = = = = = = = ="

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "RUNNING DECNET NETGEN"
puts ""

replytoprompt ">" "MOU DL1:NETGEN"
replytoprompt ">" "SET /UIC=\[137,10\]"
replytoprompt ">" "@DL1:NETGEN"

waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, E-Exit \[S\]: " ""

# NET - Section  1 - General Initialization

replytoprompt "01.00 Do you want to see the NETGEN notes/cautions \[D=N\]? \[Y/N\]: " "N"
replytoprompt "02.00 Target system device \[dduu, D=SY:\] \[S\]: " ""
replytoprompt "03.00 Listing/map device \[dduu, D=None\] \[S\]: " ""
replytoprompt "04.00 UIC Group Code for NETGEN output \[O R:1-377 D:5\]: " ""
waitforstring "07.00 User ID to be used to identify your new responses"
replytoprompt "\[D=None\] \[S R:0.-30.\]: " "ZYNQ_DEUNA"
replytoprompt "08.00 Is this generation to be a dry run \[D=N\]? \[Y/N\]: " ""
replytoprompt "09.00 Do you want a standard function network \[D=N\]? \[Y/N\]: " "N"
replytoprompt "10.00 Should all components be generated \[D=N\]? \[Y/N\]: " "N"
replytoprompt "11.00 Should old files be deleted \[D=N\]? \[Y/N\]: " "Y"
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  2 - Define the target system

replytoprompt "02.00 RSXMC.MAC location (ddu:\[g,m\], D=SY:\[011,010\]) \[S\]: " ""
replytoprompt "04.00 RSX11M.STB location (ddu:\[g,m\], D=SY00:\[001,054\]) \[S\]: " ""
replytoprompt "06.00 Should tasks link to the Memory Resident FCS library \[D=N\]? \[Y/N\]: " "Y"
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  3 - Define the system lines

replytoprompt "01.00 Device Driver Process name \[<RET>=Done\] \[S R:0-3\]: " "UNA"
replytoprompt "02.00 How many UNA controllers are there \[D R:1.-16. D:1.\]: " ""
replytoprompt "03.01 CSR address for UNA-0 \[O R:160000-177777 D:174510\]: " ""
replytoprompt "03.02 Vector address for UNA-0 \[O R:0-774 D:120\]: " ""
replytoprompt "03.03 Device priority for UNA-0 \[O R:4-6 D:5\]: " ""
replytoprompt "04.07 Set the state for UNA-0 ON when loading the network \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "01.00 Device Driver Process name \[<RET>=Done\] \[S R:0-3\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  4 - Define the CEX System

replytoprompt "01.00 Base address for partition CEXPAR \[O R:14000-113700 D:113500\]: " ""
replytoprompt "01.01 Length of partition CEXPAR \[O R:4100-4300 D:4300\]: " ""
replytoprompt "02.00 Do you want network event logging \[D=N\]? \[Y/N\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  5 - Define the Comm Exec Support Components

waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  6 - Define the System Management Utilities

replytoprompt "08.00 Do you want EVF \[D=N\]? \[Y/N\]: " "Y"
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  7 - Define the CEX Products

# DEC - Section  1 - Define the target and remote nodes

replytoprompt "01.00 What is the target node name \[S R:0-6\]: " "PDP11"
replytoprompt "02.00 What is the target node address \[S R:0.-8.\]: " "1.11"
replytoprompt "03.00 Target node ID \[D=None\] \[S R:0.-32.\]: " "PDP11"
replytoprompt "04.00 Do you want to generate a routing node \[D=N\]? \[Y/N\]: " ""
replytoprompt "06.00 Do you want to include this extended network support \[D=Y\]? \[Y/N\]: " ""
replytoprompt "07.00 Remote node name \[<RET>=Done\] \[S R:0-6\]: " "PDPI"
replytoprompt "07.01 What is the remote node address \[D=1.1\] \[S R:0.-8.\]: " "1.1"
replytoprompt "07.00 Remote node name \[<RET>=Done\] \[S R:0-6\]: " ""
replytoprompt "08.00 Do you want the language interface libraries \[D=N\]? \[Y/N\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# DEC - Section  2 - Define the DECnet Communications Components

replytoprompt "05.01 Should NETACP be checkpointable \[D=N\]? \[Y/N\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# DEC - Section  3 - Define the DECnet Network Management Components

replytoprompt "02.00 Do you want NICE \[D=N\]? \[Y/N\]: " ""
replytoprompt "03.00 Do you want EVR \[D=N\]? \[Y/N\]: " ""
replytoprompt "04.00 Do you want NTD \[D=N\]? \[Y/N\]: " ""
replytoprompt "05.00 Do you want NTDEMO \[D=N\]? \[Y/N\]: " ""
replytoprompt "06.00 Do you want LIN \[D=N\]? \[Y/N\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# DEC - Section  4 - Define the DECnet Satellite Support Components

replytoprompt "03.00 Do you want DLL \[D=N\]? \[Y/N\]: " ""
replytoprompt "05.00 Do you want CCR \[D=N\]? \[Y/N\]: " ""
replytoprompt "06.00 Do you want HLD \[D=N\]? \[Y/N\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# DEC - Section  5 - Define the DECnet File Utilities

replytoprompt "02.00 Do you want NFT \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "04.00 Do you want FAL \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "04.01 Should FAL support RMS file access \[D=Y\]? \[Y/N\]: " "N"
replytoprompt "04.02 Should FAL be overlaid \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "04.05 Number of incoming connections to support \[D R:1.-10. D:4.\]: " ""
replytoprompt "04.06 User data buffer size \[D R:260.-1024. D:1024.\]: " ""
replytoprompt "05.00 Do you want MCM \[D=N\]? \[Y/N\]: " "Y"
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# DEC - Section  6 - Define the DECnet Terminal and Control Utilities

replytoprompt "02.00 Do you want RMT/RMTACP \[D=N\]? \[Y/N\]: " ""
replytoprompt "03.00 Do you want HT:/RMHACP \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "03.01 Number of incoming connections to support \[D R:1.-16. D:4.\]: " ""
replytoprompt "04.00 Do you want NCT \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "05.00 Do you want RTH \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "06.00 Do you want TLK \[D=N\]? \[Y/N\]: " ""
replytoprompt "07.00 Do you want LSN \[D=N\]? \[Y/N\]: " ""
replytoprompt "10.00 Do you want TCL \[D=N\]? \[Y/N\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " "R"

# DEC - Section  6 - Define the DECnet Terminal and Control Utilities

replytoprompt "02.00 Do you want RMT/RMTACP \[D=N\]? \[Y/N\]: " ""
replytoprompt "03.00 Do you want HT:/RMHACP \[D=N\]? \[Y/N\]: " ""
replytoprompt "04.00 Do you want NCT \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "05.00 Do you want RTH \[D=N\]? \[Y/N\]: " "Y"
replytoprompt "06.00 Do you want TLK \[D=N\]? \[Y/N\]: " ""
replytoprompt "07.00 Do you want LSN \[D=N\]? \[Y/N\]: " ""
replytoprompt "10.00 Do you want TCL \[D=N\]? \[Y/N\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  8 - Complete the CEX System Definitions

replytoprompt "02.00 What is the Large Data Buffer (LDB) size \[D R:192.-1484. D:292.\]: " ""
waitforstring "<EOS>  Do you want to:"
replytoprompt "<RET>-Continue, R-Repeat section, P-Pause, E-Exit \[S\]: " ""

# NET - Section  9 - Build the CEX System at 12:47:45 on 14-JUL-25

# NET - Section 10 - Generation Clean Up

waitforstring ">@ <EOF>"
waitforcrlf

## rsxdisks-cp12.tgz

puts "= = = = = = = = = = = = = = = ="
puts "tar czvfC /mdsk/mrieker/rsxdisks-cp12.tgz /mdsk/mrieker rsxdisks"
exec tar czvfC /mdsk/mrieker/rsxdisks-cp12.tgz /mdsk/mrieker rsxdisks < /dev/null > /dev/tty
puts "= = = = = = = = = = = = = = = ="

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "STARTING DECNET"
puts ""

replytoprompt ">" "SET /NETUIC=\[5,54\]"
replytoprompt ">" "SET /UIC=\[5,1\]"
replytoprompt ">" "@NETINS"
replytoprompt "Do you want to install and load the CEX system? \[Y/N\]: " "Y"
replytoprompt "Do you want to install and start DECnet? \[Y/N\]: " "Y"
replytoprompt "On what device are the network tasks \[D=DL0:\] \[S\]: " ""

waitforstring ">@ <EOF>"
waitforcrlf

## ch 9: p202

## exec -ignorestderr ./z11dl < /dev/tty > /dev/tty
