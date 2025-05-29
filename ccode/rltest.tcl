#
#  Run RL test on simulator
#
#  $ ../MACRO11/src/macro11 -o rltest.obj -l rltest.lst rltest.mac
#  $ ./z11ctrl
#  > source rltest.tcl
#     on another screen:
#       $ ./z11rl -loadrw 0 ~/disk0.rl02 -loadrw 1 ~/disk1.rl02 -loadrw 2 ~/disk2.rl02 &
#       $ ./z11dl -cps 960
#  > flickstart 0400
#
pin set fpgamode 1
pin set dl_enable 1
pin set bm_enablo 0xffffffff bm_enabhi 0x3fffffff
hardreset
loadlst rltest.lst
pin set sl_enable 1 sl_switches 7
puts ""
puts "  ./z11rl -loadrw 0 ~/disk0.rl02 -loadrw 1 ~/disk1.rl02 -loadrw 2 ~/disk2.rl02 &"
puts "  ./z11dl"
puts ""
puts "  flickstart 0400"
puts ""
