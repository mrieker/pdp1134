pin set fpgamode 1
pin set bm_enablo 0xFFFFFFFF
pin set dl_enable 1 dz_addres 0760100 dz_intvec 0300 dz_enable 1
pin set ky_enable 1 ky_switches 7
hardreset
loadlst dztest.lst
flickstart 0400
