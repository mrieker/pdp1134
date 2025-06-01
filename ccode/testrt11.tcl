#
#  Boot RT-11
#  Files needed:
#    rt11.rl02
#    rlboot.lst
#

# requires PDP with real memory
proc rt11initreal {} {
    pin set fpgamode 2                  ;# select real PDP
    hardreset                           ;# reset processor
    pin set bm_enablo 0 bm_enabhi 0     ;# disable FPGA memory
    pin set kl_enable 1 sl_enable 1     ;# enable FPGA line clock, switches & lights
    exec ./z11rl -killit -loadrw 0 rt11.rl02 &
    exec ./z11dl -killit -nokb -cps 960 > /dev/tty &
}

# uses simmer.cc (x86_64) or sim1134.v (zynq) simulator
# - must start simmer.x86_64 first if on x86_64
proc rt11initsim {} {
    pin set fpgamode 1                  ;# select simulator (x86_64 - simmer; zynq - sim1134.v)
    hardreset                           ;# reset processor
    pin set bm_enablo 0xFFFFFFFF        ;# enable FPGA memory (000000..377777)
    pin set bm_enabhi 0x3FFFFFFF        ;# ... (400000..757777)
    pin set kl_enable 1 sl_enable 1     ;# enable FPGA line clock, switches & lights
    exec ./z11rl -killit -loadrw 0 rt11.rl02 &
    exec ./z11dl -killit -nokb -cps 960 > /dev/tty &
}

# do the boot
proc rt11boot {} {
    hardreset           ;# reset processor
    loadlst rlboot.lst  ;# load boot program
    flickstart 010000   ;# reset processor again and start
}
