
# Run SYSGEN for RSX system
# Set to REAL or SIM mode before running

# Assumes files from https://ftp.trailing-edge.com/pub/rsx_disks/rsx4_0/ are in $HOME/rsx40
# ... and there is a $HOME/rsx40/unzipall.sh script to unzip the .gz files to .rl01 files

#  $ ./z11ctrl
#  > pin set fpgamode 1 (for SIM) or 2 (for REAL)
#  > source testsysgen.tcl

# set sim mode if fpga turned off
puts "reset fpga"
set fm [pin fpgamode]
if {$fm == 0} {
    pin set fpgamode 1
} else {
    # as close as we can come to reset without rebooting arm
    pin set fpgamode 0 fpgamode $fm
}

# stop processor if it's babbling away
puts "reset processor"
hardreset

# make sure not processing tty i/o externally
puts "kill z11dl"
catch {exec -ignorestderr killall z11dl.armv7l < /dev/null > /dev/tty 2> /dev/tty } results options

# make sure RL01/2 drives unloaded
puts "unload rl drives"
rlunload 0
rlunload 1
rlunload 2
rlunload 3
after 1000

# reset RL01 disk images and load them on drives
puts "reset rl images"
set home [getenv HOME /tmp]
exec -ignorestderr $home/rsx40/unzipall.sh
after 1000
puts "load rl drives"
rlload 0 $home/rsx40/rsxm32.rl01
rlload 1 $home/rsx40/excprv.rl01
after 1000

# turn on RL-11 fastio mode (skip usleeps)
# turbo only works for sim (skip msyn/ssyn deskewing), ignored for real pdp
puts "enable devices"
pin set rl_fastio 1
pin set turbo 1

# make sure devices enabled and we have some memory
pin set dl_enable 1
pin set dz_enable 1 dz_addres 0760100 dz_intvec 0300
pin set kw_enable 1 kw_fiftyhz 0
pin set ky_enable 1
pin set pc_enable 1
pin set rl_enable 1
pin set tm_enable 1
enabmem

# boot RL-11 drive
puts "boot rl drive 0"
source boots.tcl
rlboot

# process prompts
puts "process prompts"
replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
replytoprompt ">" "INS \$BOO"
replytoprompt ">" "BOO \[1,54\]RSX11M"
replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
replytoprompt ">" "@SYSGEN"
replytoprompt "Autoconfigure the host system hardware? \[Y/N\]: " "Y"
replytoprompt "Do you want to override Autoconfigure results? \[Y/N\]: " "N"
replytoprompt "Do you want to inhibit execution of MCR commands? \[Y/N\]: " "N"
replytoprompt "Have you made a copy of the distribution kit? \[Y/N\]: " "Y"
replytoprompt "Are you generating a mapped system? \[Y/N\]: " "Y"
replytoprompt "Can the files in \[1,50\] be deleted? \[Y/N\]: " "Y"
replytoprompt "Use an input saved answer file? \[Y/N\]: " "N"
replytoprompt "Do you want a Standard Function System? \[Y/N\]: " "Y"
replytoprompt "Name of output saved answer file \[D: SYSSAVED.CMD\] \[S\]: " ""
replytoprompt "Clean up files from previous GENs? \[Y/N\]: " "Y"
replytoprompt "Chain to Phase II after Phase I completes? \[Y/N\]: " "Y"
replytoprompt "Enter device for EXCPRV disk when it is ready (ddu:) \[D: DL1:\] \[S\]: " ""
replytoprompt "Line frequency:   A- 60 Hz    B- 50 Hz   \[D: A\] \[S\]: " ""
replytoprompt "Highest interrupt vector \[O R:0-774 D:0\]: " ""
replytoprompt "Devices \[S\]: " "."
replytoprompt "Is a line printer available? \[Y/N\]: " "N"
replytoprompt "Does the listing/map device have at least 120 columns? \[Y/N\]: " "Y"
replytoprompt "Assembly listings device (ddu:) \[D: \"NL:\"\] \[S\]: " ""
replytoprompt "Map device for Executive and device drivers (ddu:) \[D: DL1:\] \[S\]: " ""
replytoprompt "Executive Debugging Tool (XDT)? \[Y/N\]: " "N"
replytoprompt "Include support for communications products? \[Y/N\]: " "N"
replytoprompt "Enter CDA memory dump device mnemonic (ddu:) \[S R:3-4\]: " "DL1:"
replytoprompt "Enter CDA memory dump device CSR \[O R:160000-177700 D:174400\]: " ""
replytoprompt "What name would you like to give your system \[D: RSX11M\] \[S R:0-6\]: " ""
replytoprompt "MT controller 0 \[D: 224,172522\]              \[S\]: " "224,172522,2"
waitforstring "End of SYSGEN phase II at"
waitforstring ">@ <EOF>"
replytoprompt ">" "BOO \[1,54\]RSX11M"
waitforstring "RSX11M V4.0 BL32"
replytoprompt ">" ""
replytoprompt ">" "TIM [rsxdatetime]"
replytoprompt ">" "SAV /WB"
waitforstring "RSX-11M V4.0 BL32"
replytoprompt "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) \[S\]: " [rsxdatetime]
replytoprompt "ENTER LINE WIDTH OF THIS TERMINAL \[D D:132.\]: " ""
waitforstring ">@ <EOF>"
waitforcrlf

puts ""
puts "use './z11dl -cps 960' to further access tty"

