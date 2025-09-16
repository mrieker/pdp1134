#
#  run 11/34 processor, cache, floatingpoint, memory diagnostics
#  runs diagnostics continuously
#  takes about 18min to run through one loop
#
#  download disks/xxdp22.rl02 from https://pdp-11.org.ru/en/files.pl
#
#  ./z11ctrl -log rundiags.log rundiags.tcl
#

# reboot xxdp monitor and start the given program
proc startxx {prog} {
    hardreset               ;# make sure processor halted
    rlload 0 -readonly disks/xxdp22.rl02
    rlboot                  ;# boot the xxdp image
    replytoprompt "ENTER DATE (DD-MMM-YY): " [string toupper [clock format [clock seconds] -format "%d-%b-99"]]
    replytoprompt "\n." "R $prog"
}

# wait for the given number of bells from the test program
proc waitforbells {n} {
    for {set i 0} {$i < $n} {incr i} {
        if {[ctrlcflag] || [pin ky_halted]} break
        waitforstring "\007"
        puts -nonewline "<DING>"
    }
}

# tell snapregs() to buzz off cuz it messes with this diagnostic
# snapregs reads processor registers while processor is running
proc blocksnapregs {} {
    lockdma
    pin set ky_snapblk 1
    unlkdma
}

proc allowsnapregs {} {
    lockdma
    pin set ky_snapblk 0
    unlkdma
}

# script start here #

if {[pin fpgamode] == 0} {  ;# see if fpga turned off
    pin set fpgamode 2      ;# default to real mode if not turned on
}
set fpgamode [pin fpgamode]
pin set rl_fastio 1         ;# don't do rl i/o delays
hardreset                   ;# make sure processor halted and reset
source boots.tcl            ;# get some boot functions
probedevsandmem             ;# make sure we have devices and memory
pin set ky_switches 0       ;# set switches to all off
dllock -killit              ;# take control of DL-11

set passno 0

while {! [ctrlcflag]} {
    puts ""
    puts "= = = = = = = = = = = = ="
    puts " [clock format [clock seconds] -format %H:%M:%S] MASTER PASS [incr passno]"
    puts "= = = = = = = = = = = = ="

    # processor tests

    startxx FKAA??
    waitforstring "END PASS 5"
    waitforcrlf

    blocksnapregs
    startxx FKAB??
    for {set i 0} {$i < 5} {incr i} {
        waitforstring "11/34 TRAPS TST DONE"
    }
    waitforcrlf
    allowsnapregs

    startxx FKAC??
    for {set i 0} {$i < 5} {incr i} {
        waitforstring "END PASS"
    }
    waitforcrlf

    # memory management tetsts

    startxx FKTA??
    waitforbells 5

    startxx FKTB??
    waitforbells 5

    startxx FKTC??
    waitforbells 5

    blocksnapregs
    startxx FKTD??
    waitforbells 5
    allowsnapregs

    startxx FKTG??
    waitforbells 5

    startxx FKTH??
    waitforstring "END PASS #     2"
    waitforcrlf

    # generic memory test

    startxx ZQMC??
    waitforstring "END PASS #     1"
    puts ""

    # cache test

    if {$fpgamode == 2} {
        startxx FKKA??
        replytoprompt "TYPE 'Y' OR 'N' :       " "Y"
        replytoprompt "CACHE=> " "RUN"
        for {set i 0} {$i < 5} {incr i} {
            waitforstring "PASS COMPLETE"
        }
        waitforcrlf
    }

    # floatingpoint tests

    startxx FFPA??
    waitforstring "END PASS #     5"
    waitforcrlf

    startxx FFPB??
    waitforstring "END PASS #     5"
    waitforcrlf

    startxx FFPC??
    waitforstring "END PASS #     5"
    waitforcrlf
}
