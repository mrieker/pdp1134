#
#  Run RL test on simulator or real PDP
#
#  $ ../MACRO11/src/macro11 -o rltest.obj -l rltest.lst rltest.mac
#  $ ./z11ctrl
#  > source rltest.tcl
#     on another screen:
#       $ ./z11rl -loadrw 0 /home/mrieker/disk0.rl02 -loadrw 1 /home/mrieker/disk1.rl02 -loadrw 2 /home/mrieker/disk2.rl02 &
#       $ ./z11dl -cps 960
#  > flickstart 0400
#
pin set fpgamode 2                                  ;# 1=sim; 2=real
pin set bm_enablo 0xffffffff bm_enabhi 0x3fffffff   ;# fpga memory enable
pin set dl_enable 1
hardreset
loadlst rltest.lst
pin set ky_enable 1 ky_switches 7
exec ./z11rl -killit -loadrw 0 /home/mrieker/disk0.rl02 -loadrw 1 /home/mrieker/disk1.rl02 -loadrw 2 /home/mrieker/disk2.rl02 &
exec ./z11dl -killit -nokb -cps 960 &
after 3000
flickstart 0400
