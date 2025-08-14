
# boot RSX 4.0 in disks/rsx40/rsxm32.rl01
#  ./z11ctrl rsx40run-rl01.tcl

source boots.tcl

if {[pin get fpgamode] == 0} {
    pin set fpgamode 1
}

hardreset

probedevsandmem
pin set turbo 1 rh_fastio 1 rl_fastio 1 tm_fastio 1

rlload 0 disks/rsx40/rsxm32.rl01
rlboot

exec -ignorestderr ./z11dl -killit < /dev/tty > /dev/tty
