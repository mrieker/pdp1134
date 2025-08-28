#
#  boot xxdp22 rl02 image
#  downloadable from https://pdp-11.org.ru/en/files.pl
#
#  runs in sim unless already set to real mode
#    real pdp:
#      $ ./z11ctrl
#      z11ctrl> pin set fpgamode 2
#      z11ctrl> source xxdp22.tcl
#
if {[pin fpgamode] == 0} {  ;# see if fpga turned off
    pin set fpgamode 1      ;# default to sim mode if not turned on
}
pin set turbo 1
hardreset                   ;# make sure processor halted and reset
source boots.tcl            ;# get some boot functions
probedevsandmem             ;# make sure we have devices and memory
rlload 0 -readonly disks/xxdp22.rl02
rlboot                      ;# boot the xxdp image
exec -ignorestderr ./z11dl -killit < /dev/tty > /dev/tty
