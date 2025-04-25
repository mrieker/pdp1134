
proc muxall {} {
    pin set man_rsel1_h 1 man_rsel1_h 0 man_rsel2_h 1 man_rsel2_h 0 man_rsel3_h 1 man_rsel3_h 0
}

proc rdmem {addr} {
    if {[pin get fpgamode] != 3} {
        error "fpgamode != FP_MAN"
    }
    if {[pin get hltgr_in_l]} {
        error "processor not halted"
    }
    set reply [pin set man_bbsy_out_h 1 man_a_out_h $addr man_c_out_h 0 man_msyn_out_h 1 get ssyn_in_h d_in_l]
    if {[lindex $reply 0] != 1} {
        error "reply $reply"
    }
    if {[pin set man_msyn_out_h 0 man_a_out_l 0 get ssyn_in_h set man_bbsy_out_h 0] != 0} {
        error "ssyn stuck on"
    }
    return [lindex $reply 1]
}

proc wrmem {addr data} {
    if {[pin get fpgamode] != 3} {
        error "fpgamode != FP_MAN"
    }
    if {[pin get hltgr_in_l]} {
        error "processor not halted"
    }
    set reply [pin set man_bbsy_out_h 1 man_a_out_h $addr man_c_out_h 2 man_d_out_h $data man_msyn_out_h 1 get ssyn_in_h]
    if {$reply != 1} {
        error "reply $reply"
    }
    if {[pin set man_msyn_out_h 0 man_a_out_l 0 man_c_out_l 0 man_d_out_l 0 get ssyn_in_h set man_bbsy_out_h 0] != 0} {
        error "ssyn stuck on"
    }
}
