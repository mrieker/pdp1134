if {[pin fpgamode] == 0} {  ;# see if turned on
    pin set fpgamode 1      ;# turn on simulator
}
dllock -killit              ;# lock access to simulated DL-11
source boots.tcl
probedevsandmem             ;# make sure devs and mem enabled
loadlst fptest.lst          ;# load test program
hardreset                   ;# reset and read power-up vector
flickcont                   ;# let it run
while {! [ctrlcflag] && ! [pin ky_halted]} {
    set ch [readttychar 500]
    puts -nonewline $ch     ;# echo tty output till it halts
    flush stdout
}
