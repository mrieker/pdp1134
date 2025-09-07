pin set fpgamode 1
hardreset
dllock -killit              ;# lock access to simulated DL-11
pin set bm_enablo 0xFFFF dl_enable 1 ky_enable 1
pin set ky_switches 0$argv  ;#  100000 = halt on error
loadbin ../maindec/fpdiag/FFPBA0.BIN
puts "switches = [octal [pin ky_switches]]"
flickstart 0200
set line ""
while {! [ctrlcflag] && ! [ishalted]} {
    set ch [readttychar 500]
    if {$ch == "\n"} {
        puts $line
        set line ""
    } elseif {$ch != ""} {
        append line $ch
    } elseif {$line != ""} {
        puts -nonewline $line
        flush stdout
        set line ""
    }
}
