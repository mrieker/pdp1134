#
#  Boot RL01/2 Drive 0 (real or fake)
#
#  Runs with sim or real PDP
#  Real PDP can have console board M7859 plugged in and cabled to console
#  Real PDP can have boot rom board M9312 plugged in and cabled to console
#  ...but M9312 SW 1-2 must be OFF so it doesn't interfere with our startup
#
#  $ ./z11ctrl
#  > pin set fpgamode 2             (or 1 for sim)
#  > pin set bm_enablo 0xFFFFFFFF   (if real PDP with no memory)
#  > pin set bm_enablo 0            (if real PDP with real memory)
#  > rlload 0 somediskimage.rl02    (if using fake RL02)
#  > rlload 1 someotherimage.rl02   (optional, if using fake RL02)
#  > rlboot.tcl
#  > exit
#  $ ./z11dl -cps 960               (unless using a real DL-11)
#

# start program in fpga boot memory
# assumes fpga boot memory loaded with power-up vector pointing to program
# if real PDP with M9312 boot card, S1-2 must be OFF so it won't stomp on us
proc startbootmem {} {
    pin set man_ac_lo_out_h 1                   ;# power down the processor
    after 5
    pin set man_dc_lo_out_h 1
    pin set ky_haltreq 0                        ;# make sure fpga console (ky11.v) isn't requesting halt
    pin set bm_brjame 1                         ;# enable jamming 761xxx address on the Unibus
    after 200                                   ;# give PDP circuits time to reset
    pin set man_dc_lo_out_h 0 man_ac_lo_out_h 0 ;# tell PDP the power is back
}

# probe to see if a csr can be read from unibus
#  input:
#   csr = 16-bit address of csr to probe
#  output:
#   returns true: csr can be read
#          false: timed out trying to read csr
proc probecsr {csr} {

    # assume worst-case scenario: real pdp with real ky-11 console in place
    # it blocks dma by holding bbsy asserted
    # so we put a program in fpga (bigmem.v) boot memory without using unibus
    # and it probes the csr returning the result in fpga boot memory
    bmwrword 0761024 0161100        ;# power-up vector
    bmwrword 0761026 0000340
    bmwrword 0761100 0000240        ;# nop (debug)
    bmwrword 0761102 0012737        ;# mov #161200,@#0000004
    bmwrword 0761104 0161200
    bmwrword 0761106 0000004
    bmwrword 0761110 0012737        ;# mov #000340,@#0000006
    bmwrword 0761112 0000340
    bmwrword 0761114 0000006
    bmwrword 0761116 0012706        ;# mov #161700,r6
    bmwrword 0761120 0161700
    bmwrword 0761122 0005737        ;# tst @#csr
    bmwrword 0761124 $csr
    bmwrword 0761126 0012737        ;# mov #1,@#161000
    bmwrword 0761130 0000001
    bmwrword 0761132 0161000
    bmwrword 0761134 0000000        ;# halt
    bmwrword 0761136 0000776        ;# br  .-2
    bmwrword 0761200 0012737        ;# mov #2,@#161000
    bmwrword 0761202 0000002
    bmwrword 0761204 0161000
    bmwrword 0761206 0000000        ;# halt
    bmwrword 0761210 0000776        ;# br  .-2
    bmwrword 0761000 0              ;# clear flag word
    startbootmem                    ;# start it up
    after 200                       ;# give it plenty of time to run
                                    ;# (150mS barely enough)
    set flag [bmrdword 0761000]     ;# read flag word
    puts "probecsr: csr=[octal $csr] flag=$flag"
    switch $flag {
        0 {error "probecsr: failed to run"}
        1 {return true}
        2 {return false}
        default {error "probecsr: unknown result"}
    }
}

## SCRIPT STARTS HERE ##

set fpgamode [pin fpgamode]
if {($fpgamode != 1) && ($fpgamode != 2)} {
    puts "rlboot.tcl: fpga OFF - set SIM or REAL mode"
    return
}
if {($fpgamode == 1) && ([pin bm_enablo] == 0)} {
    pin set bm_enablo 0xFFFFFFFF    ;# make sure sim has memory
    pin set bm_enabhi 0x3FFFFFFF
}

hardreset                           ;# clobber processor in case it's babbling
pin set bm_brenab 2 bm_brjama 1     ;# set up fpga boot memory 761000..7617777

if {! [probecsr 0174400]} {         ;# see if we have rl-11 controller
    pin set rl_enable 1             ;# if not, enable fpgas rl-11 controller (rl11.v)
}

if {! [probecsr 0177546]} {         ;# see if we have line clock
    pin set kw_enable 1             ;# if not, enable fpgas line clock (kw11.v)
}

if {! [probecsr 0177560]} {         ;# see if we have console tty
    pin set dl_enable 1             ;# if not, enable fpgas tty (dl11.v)
}

if {! [probecsr 0177570]} {         ;# see if we have console switches & lights
    pin set ky_enable 1             ;# if not, enable fpgas switches & lights (ky11.v)
}

loadlst rlboot.lst bmwr
startbootmem

set ttmsg ""
if {[pin dl_enable]} {
    set ttmsg " use z11dl -cps 960 for tty access"
}

puts "rlboot.tcl: RL drive 0 booting...$ttmsg"
