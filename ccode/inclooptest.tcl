
# simple test doing increment loop

proc start {} {
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
}

proc loop {} {
    while {! [ctrlcflag]} {
        after 200

        if false {
            pin set man_npr_out_h 1
            for {set i 0} {[pin dev_npg_l]} {incr i} {
                if {$i > 100} {
                    error "loop: npg timeout"
                }
            }
            pin set man_sack_out_h 1 man_npr_out_h 0 man_bbsy_out_h 1 man_sack_out_h 0
            pin set man_a_out_h 0200 man_msyn_out_h 1
            for {set i 0} {! [pin dev_ssyn_h]} {incr i} {
                if {$i > 100} {
                    error "loop: ssyn timeout"
                }
            }
            set value [pin get dev_d_h]
            pin set man_msyn_out_h 0
            pin set man_a_out_h 0
            pin set man_bbsy_out_h 0
        } else {
            set value [rdword 0200]
        }

        set hhmmss [clock format [clock seconds] -format %H:%M:%S]
        puts "$hhmmss [octal $value]"
    }
}
