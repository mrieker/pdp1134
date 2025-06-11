
pin set bm_brenab 2         ;# enable 761000..761777
loadlst brtest.lst          ;# load test program
pin set bm_brjama 1         ;# set up to jam 761000 on addr bus
pin set man_ac_lo_out_h 1   ;# power fail
after 10
pin set man_dc_lo_out_h 1
pin set bm_brjame 1         ;# jam 761000 on addr bus
after 100
pin set man_dc_lo_out_h 0   ;# power up
pin set man_ac_lo_out_h 0

