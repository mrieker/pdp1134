pin set fpgamode 1
hardreset
dllock -killit              ;# lock access to simulated DL-11
pin set bm_enablo 0xFFFF dl_enable 1 ky_enable 1
pin set ky_switches 0$argv  ;#  100000 = halt on error
loadbin ../maindec/fpdiag/FFPAA1.BIN
puts "switches = [octal [pin ky_switches]]"
flickstart 0200
while {! [ctrlcflag] && ! [pin ky_halted]} {
    set ch [readttychar 500]
    puts -nonewline $ch     ;# echo tty output till it halts
    flush stdout
}
