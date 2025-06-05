
if {[pin get fpgamode] == 0} {
    pin set fpgamode 1          ;# set sim mode by default
}
enabmem                         ;# make sure we have some memory
hardreset                       ;# clobber processor if it was babbling
puts [format "fpgamode=%u bm_enable=%08X.%08X" [pin fpgamode] [pin bm_enabhi] [pin bm_enablo]]
pin set kl_enable 1 ky_enable 1 ;# make sure lineclock and console plugged in

set home [getenv HOME /tmp]
set rsx $home/rsx11.rl02
exec ./rsx.sh $rsx &            ;# load files in disk drives
after 3000                      ;# give it time to start up

loadlst $rsx/dl.lst             ;# load bootstrap in memory
flickstart 010000               ;# start it going

exec -ignorestderr ./z11dl -cps 960 -killit < /dev/tty > /dev/tty
