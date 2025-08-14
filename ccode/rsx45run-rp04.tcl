
# boot RSX 4.5 RP04 built with rsx45sysgen-rp04.sh
# start with ./rsx45run-rp04.sh

source boots.tcl

if {[pin get fpgamode] == 0} {
    pin set fpgamode 1
}

hardreset

probedevsandmem
pin set turbo 1 rh_fastio 1 rl_fastio 1 tm_fastio 1

rhload 0 disks/rsx45-db/dbsys.rp04
rhboot

exec -ignorestderr ./z11dl -killit < /dev/tty > /dev/tty
