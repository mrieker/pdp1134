
if {[pin get fpgamode] == 0} {
    pin set fpgamode 1              ;# set sim mode by default
}
enabmem                             ;# make sure we have some memory
hardreset                           ;# clobber processor if it was babbling
puts [format "rsx.tcl: fpgamode=%u bm_enable=%08X.%08X" [pin fpgamode] [pin bm_enabhi] [pin bm_enablo]]
pin set dl_enable 1                 ;# plug console tty board in
pin set kw_fiftyhz 1 kw_enable 1    ;# plug line clock board in
pin set ky_enable 1                 ;# plug 777570 light/switch register board in

set home [getenv HOME /tmp]
set rsx $home/rsx11.rl02
puts "rsx.tcl: loading disks"
exec -ignorestderr ./z11rl -killit \
    -loadrw 0 $rsx/rsx11m4.1_sys_34.rl02 \
    -loadrw 1 $rsx/rsx11m4.1_user.rl02   \
    -loadrw 2 $rsx/rsx11m4.1_hlpdcl.rl02 \
    -loadrw 3 $rsx/rsx11m4.1_excprv.rl02 &
after 3000

loadlst $rsx/dl.lst                 ;# load bootstrap in memory
flickstart 010000                   ;# start it going

puts "rsx.tcl: rsx booting... use z11dl to access tty"
