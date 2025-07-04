#
#  Run RL test on simulator or real PDP
#
#  $ ../MACRO11/src/macro11 -o rltest.obj -l rltest.lst rltest.mac
#  $ ./z11ctrl rltest.tcl
#
pin set fpgamode 1                                  ;# 1=sim; 2=real
pin set bm_enablo 0xffffffff bm_enabhi 0x3fffffff   ;# fpga memory enable
pin set dl_enable 1
hardreset
loadlst rltest.lst
pin set ky_enable 1 ky_switches 7
set home [getenv HOME /tmp]
rlload 0 $home/disk0.rl02
rlload 1 $home/disk1.rl02
rlload 2 $home/disk2.rl02
exec ./z11dl -killit -nokb -cps 960 &
after 3000
flickstart 0400
