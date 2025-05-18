
# simple test doing increment loop

pin set fpgamode 2
hardreset
pin set bm_enablo 0xFFFFFFFF bm_enabhi 0x3FFFFFFF

wrword 0100 05200   ;# inc r0
wrword 0102 01376   ;# bne 0100
wrword 0104 05237   ;# inc @#0200
wrword 0106 00200
wrword 0110 00773   ;# br 0100

wrword 024 0100
wrword 026 0340

pin set man_ac_lo_out_h 1 man_dc_lo_out_h 1
pin set sl_haltreq 0
pin set man_dc_lo_out_h 0 man_ac_lo_out_h 0

