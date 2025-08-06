
# generate rsts/e v10.1 system from tapes onto an rp04

# input:
#  disks/rststapes/rsts101/rstse_v10_1_install_sep10_1992.tap
#    from https://groups.google.com/g/pidp-11/c/kStWvbpQuUA/m/8BYBQrCgAgAJ
#      about 1/2 way down - link RSTS/E 10.1

# output:
#  disks/rsts101/dbsys.rp04

# note:
#  disks should be softlink to magnetic disk so it doesn't
#  pound the daylights out of the sdcard

proc replypwprompt {prompt password} {
    waitforstring $prompt
    set len [string length $password]
    for {set i 0} {$i < $len} {incr i} {
        after 500
        set ch [string index $password $i]
        sendttychar $ch
        puts -nonewline $ch
        flush stdout
    }
    after 500
    sendttychar "\r"
}

if {[pin get fpgamode] == 0} {
    pin set fpgamode 1
}

hardreset       ;# make sure processor is stopped
dllock -killit  ;# take over console

rhunload 0      ;# make sure nothing loaded in these drives
tmunload 0

exec rm -rf disks/rsts101
exec mkdir -p disks/rsts101

rhload 0 -create disks/rsts101/dbsys.rp04
tmload 0 -readonly disks/rststapes/rsts101/rstse_v10_1_install_sep10_1992.tap

source boots.tcl                        ;# get boot functions
probedevsandmem                         ;# turn on all devices
pin set turbo 1 rh_fastio 1 tm_fastio 1 ;# no seek or search delays
tmboot                                  ;# boot magtape

set nowbin  [clock seconds]
set nowdate [string toupper [clock format $nowbin -format "%d-%b-99"]]
set nowtime [clock format $nowbin -format "%H:%M"]

replytoprompt "Today's date? " $nowdate
replytoprompt "Current time? " $nowtime
replytoprompt "Installing RSTS on a new system disk? <Yes> " ""
replytoprompt "Disk? " "DB0"
replytoprompt "Pack ID? " "RSTS10"
replytoprompt "Pack cluster size <4>? " ""
replytoprompt "MFD cluster size <16>? " ""
replytoprompt "SATT.SYS base <21472>? " ""
replytoprompt "Pre-extend directories <NO>? " ""
replytoprompt "PUB, PRI, or SYS <SYS>? " ""
replytoprompt "\[1,1\] cluster size <16>? " ""
replytoprompt "\[1,2\] cluster size <16>? " ""
replytoprompt "\[1,1\] and \[1,2\] account base <21472>? " ""
replytoprompt "Date last modified <YES>? " ""
replytoprompt "New files first <NO>? " ""
replytoprompt "Read-only <NO>? " ""
replytoprompt "Format <NO>? " ""
replytoprompt "Patterns <3>? " "0"
replytoprompt "Erase Disk <YES>? " "NO"
replytoprompt "Proceed (Y or N)? " "Y"
replytoprompt "Installation device? <_MT0:> " ""
replytoprompt "Are you ready to proceed? <yes> " ""
replytoprompt "Target disk? <_SY:> " ""
replytoprompt "Do you want to install the RSTS/E monitor? <yes> " ""
replytoprompt "Please type A or B <A> " ""
replytoprompt "Use template monitor ?          <N >    " ""
replytoprompt "New Monitor name ?              <RSTS>  " ""
replytoprompt "Accept defaults ?               <N >    " ""
replytoprompt "Accept Disk defaults ?          <N >    " ""
replytoprompt "RK05's ?                        <00>    " ""
replytoprompt "RL01/RL02's ?                   <04>    " "2"
replytoprompt "Overlapped seek ?               <Y >    " "N"
replytoprompt "RK06/RK07's ?                   <00>    " ""
replytoprompt "Disks on DR controller ?        <00>    " ""
replytoprompt "Disks on DB controller ?        <08>    " "2"
replytoprompt "Overlapped seek ?               <Y >    " "N"
replytoprompt "MSCP support ?                  <N >    " ""
replytoprompt "Accept Peripheral defaults ?    <N >    " ""
replytoprompt "TU16/TE16/TU45/TU77's ?         <00>    " ""
replytoprompt "TU10/TE10/TS03's ?              <08>    " "2"
replytoprompt "TS11/TK25/TSV05/TU80's ?        <00>    " ""
replytoprompt "TMSCP tape drives ?             <00>    " ""
replytoprompt "Printers ?                      <00>    " ""
replytoprompt "RX01/RX02's ?                   <00>    " ""
replytoprompt "CR11/CM11 card reader ?         <N >    " ""
replytoprompt "CD11 card reader ?              <N >    " ""
replytoprompt "DMC11's/DMR11's ?               <00>    " ""
replytoprompt "DMV11's/DMP11's ?               <00>    " ""
replytoprompt "IBM 3271 or 2780/3780 simultaneous links ?      <00>    " ""
replytoprompt "RJ2780 support ?                <N >    " ""
replytoprompt "Accept Software defaults ?      <N >    " ""
replytoprompt "Maximum jobs ?                  <25>    " ""
replytoprompt "Small buffers ?                 <455>   " ""
replytoprompt "EMT Logging ?                   <N >    " ""
replytoprompt "Are you ready to proceed? <yes> " ""
replytoprompt "Do you want to install the System Program packages? <yes> " ""
 
#   AUXLIB    Auxiliary Library               PBS       Print/Batch Services
#   BASIC     Basic-Plus                      RESTORE   File Restore
#   DAP       RMS DECnet Data Access Package  RMS       RMS-11 (Includes RSX)
#              (includes RMS-11)              RSX       RSX Utilities
#   EDT       EDT Editor                      SORT      Sort/Merge (includes RMS)
#   HELP      Help Package                    TECO      TECO Editor
#   OPSER     Opser-based Spooler             TEST      Device Testing
#   OMS       Operator/Message Services       UNSUPP    Unsupported Utilities
#                                             UPDATE    Update Package

replytoprompt "Packages to install <ALL> : " "AUXLIB,BASIC,EDT,HELP,RESTORE,RSX,SORT,TECO,UNSUPP,UPDATE"
replytoprompt "Is this list OK? <yes> " ""
replytoprompt "Do you want the I&D versions of tasks? <N> " ""
replytoprompt "Use resident library version of RMS? <N> " ""
replytoprompt "Use resident library version of EDT? <N> " ""
replytoprompt "Are you ready to proceed? <yes> " ""
replytoprompt "Do you wish to do the transfer? <Yes> " ""

# BP2         DIBOL       FRTRN77     PLXY        3271
# C81         DTR         MAIL        RJ2780
# DECDX       FMS         MENU        WPS
# DECNET      FORTRAN     PDPDBG      2780 3780

replytoprompt "Product updates to transfer? <ALL> : " "ALL"
replytoprompt "Is this list OK? <yes> " ""
replytoprompt "Patch account? <PATCH\$\$:> " ""
replypwprompt "New password: " "FATCAT"
replypwprompt "New password again, for verification: " "FATCAT"
replytoprompt "Are you ready to proceed? <yes> " ""
replytoprompt "Do you want _SY0:\[0,1\]SWAP1.SYS created (at 3264 blocks)? <yes> " ""

waitforstring "Please mount volume 2 on _MT0:."
tmload 0 -readonly disks/rststapes/rsts101/rstse_v10_1_install_sep10_1992.tap
replytoprompt "Press RETURN to continue : " ""

# Beginning of RSTS/E Basic-Plus generation.
replytoprompt "New BASIC-PLUS RTS name ?       <BASIC> " ""
replytoprompt "Accept defaults ?               <N>     " "Y"
replytoprompt "Create another BASIC-PLUS RTS ? <N>     " ""

# Installing SORT/MERGE package
replytoprompt "Do you want to proceed with the default installation             <YES>? " ""
replytoprompt "ARE THE ABOVE DEFAULTS THE DESIRED ONES    <YES or NO>           <YES>? " ""

replytoprompt "Would you like to start your new monitor now? <Y> " ""
replytoprompt "Proceed with system startup? <YES> " ""

waitforstring "RSTS/E is on the air..."
waitforcrlf

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "ALL DONE"
puts ""
puts "  do ./z11dl to access tty"
puts ""
