
# install minimal RSTS/E V9.6 onto an RL02

#  ./z11gui &  (optional)
#  ./z11ctrl rsts96sysgen-rl02.tcl

# start with:
#  disks/rststapes/rsts_v9_6_install.tap
#  disks/rststapes/rsts_v9_library.tap
#  (from https://ftp.trailing-edge.com/pub/rsts_dists/
#     rsts_v9_6_install.zip
#     rsts_v9_library.zip)

#  disks/ should probably be softlink to magnetic disk

# outputs:
#  disks/rsts96/dlsys0.rl02
#  disks/rsts96/dlsys1.rl02  (unused)

source boots.tcl

proc sendpwstring {st} {
    set len [string length $st]
    after 400
    for {set i 0} {$i < $len} {incr i} {
        after 500
        set ch [string index $st $i]
        sendttychar $ch
        puts -nonewline $ch
        flush stdout
    }
}

if {[pin get fpgamode] == 0} {
    pin set fpgamode 1
}
hardreset       ;# make sure processor halted
rhunload 0      ;# make sure drives unloaded
rhunload 1
rlunload 0
rlunload 1
tmunload 0
tmunload 1
dllock -killit  ;# make sure nothing else using DL-11

exec rm -rf disks/rsts96
exec mkdir -p disks/rsts96

probedevsandmem
pin set turbo 1 rl_fastio 1 tm_fastio 1

rlload 0 -create disks/rsts96/dlsys0.rl02
rlload 1 -create disks/rsts96/dlsys1.rl02
tmload 0 -readonly disks/rststapes/rsts_v9_6_install.tap
tmboot

set nowbin  [clock seconds]
set nowdate [string toupper [clock format $nowbin -format "%d-%b-99"]]
set nowtime [clock format $nowbin -format "%H:%M"]

# Performing limited hardware scan.
replytoprompt "Today's date? " $nowdate
replytoprompt "Current time? " $nowtime
replytoprompt "Installing RSTS on a new system disk? <Yes> " ""
replytoprompt "Disk? " "DL0"
replytoprompt "Pack ID? " "RSTSV9"
replytoprompt "Pack cluster size <1>? " ""
replytoprompt "MFD cluster size <16>? " ""
replytoprompt "SATT.SYS base <10221>? " ""
replytoprompt "Pre-extend directories <NO>? " ""
replytoprompt "PUB, PRI, or SYS <SYS>? " ""
replytoprompt "\[1,1\] cluster size <16>? " ""
replytoprompt "\[1,2\] cluster size <16>? " ""
replytoprompt "\[1,1\] and \[1,2\] account base <10221>? " ""
replytoprompt "Date last modified <YES>? " ""
replytoprompt "New files first <NO>? " ""
replytoprompt "Read-only <NO>? " ""
replytoprompt "Patterns <3>? " "0"
replytoprompt "Erase Disk <YES>? " "NO"
replytoprompt "Proceed (Y or N)? " "Y"

replytoprompt "Start timesharing? <Yes> " ""
replytoprompt "Do you want to perform an installation or an update? <installation> " ""
replytoprompt "Installation device? <_MT0:> " ""

replytoprompt "Are you ready to proceed? <yes> " ""
replytoprompt "Target disk? <_SY:> " ""
replytoprompt "Do you want to install the RSTS/E monitor? <yes> " ""
replytoprompt "Use template monitor ?          <N >    " ""
replytoprompt "New Monitor name ?              <RSTS>  " ""

replytoprompt "Accept defaults ?               <N >    " ""
replytoprompt "Accept Disk defaults ?          <N >    " ""
replytoprompt "RK05's ?                        <00>    " ""
replytoprompt "RL01/RL02's ?                   <04>    " ""
replytoprompt "Overlapped seek ?               <Y >    " ""
replytoprompt "RK06/RK07's ?                   <00>    " ""
replytoprompt "RP02/RP03's ?                   <00>    " ""
replytoprompt "Disks on DR controller ?        <00>    " ""
replytoprompt "Disks on DB controller ?        <08>    " "2"
replytoprompt "Overlapped seek ?               <Y >    " ""
replytoprompt "MSCP support ?                  <N >    " ""

replytoprompt "Accept Peripheral defaults ?    <N >    " ""
replytoprompt "TU16/TE16/TU45/TU77's ?         <00>    " ""
replytoprompt "TU10/TE10/TS03's ?              <08>    " "2"
replytoprompt "TS11/TK25/TSV05/TU80's ?        <00>    " ""
replytoprompt "TMSCP tape drives ?             <00>    " ""
replytoprompt "DECtapes ?                      <00>    " ""
replytoprompt "Printers ?                      <00>    " ""
replytoprompt "RX01/RX02's ?                   <00>    " ""
replytoprompt "CR11/CM11 card reader ?         <N >    " ""
replytoprompt "CD11 card reader ?              <N >    " ""
replytoprompt "P.T. reader / punch ?           <Y >    " ""
replytoprompt "DMC11's/DMR11's ?               <00>    " ""
replytoprompt "DMV11's/DMP11's ?               <00>    " ""
replytoprompt "IBM 3271 or 2780/3780 simultaneous links ?      <00>    " ""
replytoprompt "RJ2780 support ?                <N >    " ""

replytoprompt "Accept Software defaults ?      <N >    " ""
replytoprompt "Maximum jobs ?                  <25>    " ""
replytoprompt "Small buffers ?                 <455>   " ""
replytoprompt "EMT Logging ?                   <N >    " ""

replytoprompt "Do you want to install the System Program packages? <yes> " ""
replytoprompt "Packages to install: " "EDT" ;# ,UPDATE,TEST,TECO,SORT,HELP,BASIC"
replytoprompt "Proceed? <yes> " ""
replytoprompt "Do you want the I&D versions of tasks? <no> " ""
replytoprompt "Use resident library version of EDT? <yes> " ""

waitforstring "Do you want to transfer the layered product update components"
replytoprompt "from the Installation kit? <yes> " ""
replytoprompt "Product updates to transfer: " "DECNET" ;# "DECGRAPH,DECNET,FMS,FRTRN77,F77DBG,WPS"
replytoprompt "Proceed? <yes> " ""
replytoprompt "Patch account? <PATCH\$:> " ""
waitforstring "New password: "
sendpwstring "FATCAT\r"
waitforstring "New password again, for verification: "
sendpwstring "FATCAT\r"

replytoprompt "Are you ready to proceed? <yes> " ""
replytoprompt "Do you want _SY0:SWAP1.SYS created? <yes> " ""

waitforstring "Please mount the RSTS/E Library media"
tmload 0 -readonly disks/rststapes/rsts_v9_library.tap
replytoprompt "Library device? <_MT0:> " ""

;# replytoprompt "BASIC-PLUS RTS name ?           <BASIC> " ""
;# replytoprompt "FPP ?                           <N >    " ""
;# replytoprompt "FIS ?                           <N >    " ""
;# replytoprompt "Math precision ?                <02>    " ""
;# replytoprompt "Log functions ?                 <Y >    " ""
;# replytoprompt "Trig functions ?                <Y >    " ""
;# replytoprompt "Print using ?                   <Y >    " ""
;# replytoprompt "Matrices ?                      <N >    " ""
;# replytoprompt "String arithmetic ?             <N >    " ""

;# replytoprompt "Do you want to proceed with the default installation             <YES>? " ""
;# replytoprompt "ARE THE ABOVE DEFAULTS THE DESIRED ONES    <YES or NO>           <YES>? " ""

waitforstring "is a log file of this session"
waitforcrlf

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "ALL DONE"
puts ""
puts "  do ./z11dl to access tty"
puts ""
