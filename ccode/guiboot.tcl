
# called via guiboot.sh as a result of clicking gui boot button
# can also be run manually with ./guiboot.sh

puts "guiboot.tcl: started"

source $Z11HOME/boots.tcl

# write string to tty
proc wrtty {str} {
    set len [string length $str]
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $str $i]
        if {$ch == "\n"} {wrtty "\r"}
        while {! [ctrlcflag] && ! ([rdword 0777564] & 0200)} {after 1}
        if {[ctrlcflag]} break
        scan $ch "%c" by
        wrbyte 0777566 $by
    }
    while {! [ctrlcflag] && ! ([rdword 0777564] & 0200)} {after 1}
}

# read single char from tty
proc rdtty {} {
    while {! ([rdword 0777560] & 0200)} {
        after 1
        if {[ctrlcflag]} {return 3}
    }
    return [rdbyte 0777562]
}

##  SCRIPT STARTS HERE  ##

set fpgamode [pin fpgamode]
if {($fpgamode != 1) && ($fpgamode != 2)} {
    puts "guiboot.tcl: zynq turned OFF - select REAL or SIM then press BOOT again"
    exit
}
hardreset

# make sure daemons are running for devices enabled with gui checkboxes or otherwise
startdaemons

# display message on gui message box saying which tty we are using
if {[pin dl_enable]} {
    puts "guiboot.tcl: use $Z11HOME/z11dl to access zynq tty"
} else {
    puts "guiboot.tcl: using hardware tty controller"
}

# see what boot devices the pdp can access - real or fpga
set have_pc [expr {[rdwordtimo 0777550] >= 0}]
set have_rh [expr {[rdwordtimo 0776700] >= 0}]
set have_rl [expr {[rdwordtimo 0774400] >= 0}]
set have_tm [expr {[rdwordtimo 0772520] >= 0}]

# display and process boot menu
wrtty "\n? for help > "

while {! [ctrlcflag]} {
    set by [format %03o [rdtty]]
    switch $by {
        060 {
            wrtty "0\nabandoning boot\n"
            return
        }
        061 {
            if {$have_rl} {
                wrtty "1\nbooting RL-11 drive 0\n"
                clearlow4k
                rlboot
                return
            }
        }
        062 {
            if {$have_pc} {
                wrtty "2\nbooting from paper tape bin file\n"
                prboot
                return
            }
        }
        063 {
            if {$have_rh} {
                wrtty "3\nbooting RH-11 drive 0\n"
                clearlow4k
                rhboot
                return
            }
        }
        064 {
            if {$have_tm} {
                wrtty "4\nbooting TM-11 drive 0\n"
                clearlow4k
                tmboot
                return
            }
        }
        077 {
                           wrtty "?\nguiboot.tcl: Select\n"
                           wrtty "  0) cancel boot\n"
            if {$have_rl} {wrtty "  1) boot from RL-11 drive 0\n"}
            if {$have_pc} {wrtty "  2) boot from paper tape bin file\n"}
            if {$have_rh} {wrtty "  3) boot from RH-11 drive 0\n"}
            if {$have_tm} {wrtty "  4) boot from TM-11 drive 0\n"}
                           wrtty "> "
        }
    }
}
