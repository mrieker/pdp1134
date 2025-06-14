puts "guiboot.tcl: started"

source $Z11HOME/boots.tcl

# write string to tty
proc wrtty {str} {
    set len [string length $str]
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $str $i]
        if {$ch == "\n"} {wrtty "\r"}
        while {! [ctrlcflag] && ! ([rdword 0777564] & 0200)} {after 1}
        scan $ch "%c" by
        wrbyte 0777566 $by
    }
}

# read single char from tty
proc rdtty {} {
    while {! [ctrlcflag] && ! ([rdword 0777560] & 0200)} {after 1}
    return [rdbyte 0777562]
}

##  SCRIPT STARTS HERE  ##

set fpgamode [pin fpgamode]
if {($fpgamode != 1) && ($fpgamode != 2)} {
    puts "guiboot.tcl: zynq turned OFF - select REAL or SIM then press BOOT again"
    exit
}

puts "guiboot.tcl: probing devices and memory"
probedevsandmem

if {[pin dl_enable]} {
    puts "guiboot.tcl: use z11dl -cps 960 to access zynq tty"
} else {
    puts "guiboot.tcl: using hardware tty controller"
}

wrtty "\n"
wrtty "guiboot.tcl: Select\n"
wrtty "  0) cancel boot\n"
wrtty "  1) boot from RL-11 drive 0\n"
wrtty "> "

while {! [ctrlcflag]} {
    set by [rdtty]
    if {$by == 060} {
        wrtty "0\nabandoning boot\n"
        break
    }
    if {$by == 061} {
        wrtty "1\nbooting RL-11 drive 0\n"
        rlboot
        break
    }
}
