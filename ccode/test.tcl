
proc octal {x} {return [format "%06o" $x]}

proc rdbyte {addr} {
    if {($addr < 0) || ($addr > 0777777)} {
        error "rdbyte: bad address $addr"
        error [format "rdbyte: bad address %o" $addr]
    }
    pin set sl_dmaaddr [expr {$addr & 0777776}] sl_dmactrl 0 sl_dmastate 1
    if {[pin get sl_dmafail]} {
        error [format "rdbyte %06o failed" $addr]
    }
    set data [pin get sl_dmadata]
    if {$addr & 1} {set data [expr {$data >> 8}]}
    return [expr {$data & 0377}]
}

proc rdword {addr} {
    if {($addr < 0) || ($addr > 0777777)} {
        error "rdword: bad address $addr"
        error [format "rdword: bad address %o" $addr]
    }
    if {($addr & 1) && (($addr < 0777700) || ($addr > 0777717))} {
        error [format "rdword: odd address %06o" $addr]
    }
    pin set sl_dmaaddr $addr sl_dmactrl 0 sl_dmastate 1
    if {[pin get sl_dmafail]} {
        error [format "rdword %06o failed" $addr]
    }
    return [pin get sl_dmadata]
}

proc wrbyte {addr data} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "wrbyte: bad address %o" $addr]
    }
    if {($data < 0) || ($data > 0377)} {
        error [format "wrbyte: bad data %o" $data]
    }
    if {$addr & 1} {set data [expr {$data << 8}]}
    pin set sl_dmaaddr [expr {$addr & 0777776}] sl_dmactrl 3 sl_dmadata $data sl_dmastate 1
    if {[pin get sl_dmafail]} {
        error [format "wrbyte %06o failed" $addr]
    }
}

proc wrword {addr data} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "wrword: bad address %o" $addr]
    }
    if {($addr & 1) && (($addr < 0777700) || ($addr > 0777717))} {
        error [format "wrword: odd address %06o" $addr]
    }
    if {($data < 0) || ($data > 0177777)} {
        error [format "wrword: bad data %o" $data]
    }
    pin set sl_dmaaddr $addr sl_dmactrl 2 sl_dmadata $data sl_dmastate 1
    if {[pin get sl_dmafail]} {
        error [format "wrword %06o failed" $addr]
    }
}

#######################

# test to increment R0
# prints R0 every 100mS
proc test1 {} {
    pin set fpgamode 0 fpgamode 1 lm_enab 1
    pin set sl_enable 1 sl_haltreq 1 man_hltrq_out_h 0
    wrword 0100 05200           ;# 0100: INC R0
    wrword 0102 0776            ;# 0102: BR .-2
    wrword 0777707 0100         ;# PC = 0100
    while {! [ctrlcflag]} {
        puts "  R0=[octal [rdword 0777700]]"
        pin set sl_haltreq 0
        after 100
        pin set sl_haltreq 1
    }
}

# test to print to TTY
# increments R0 and prints
proc test2 {} {
    pin set fpgamode 0 fpgamode 1 lm_enab 1
    pin set sl_haltreq 1 man_hltrq_out_h 0
    pin set dl_enable 1
    wrword 0100 0005200         ;# 0100: INC R0
    wrword 0102 0105737         ;# 0102: TSTB @#177564
    wrword 0104 0177564
    wrword 0106 0100375         ;# 0106: BPL .-4
    wrword 0110 0110037         ;# 0110: MOVB R0,@#177566
    wrword 0112 0177566
    wrword 0114 0000771         ;# 0114: BR .-12
    wrword 0777707 0100         ;# PC = 0100
    pin set sl_haltreq 0
    dumptty
}

# single step one instruction then print PC and R0
proc singlestep {} {
    pin set sl_stepreq 1
    puts "PC=[octal [rdword 0777707]] RO=[octal [rdword 0777700]]"
}

# dump what is written to tty
proc dumptty {} {
    while {! [ctrlcflag]} {
        set xcsr [pin get dl_xcsr]
        if {! ($xcsr & 0200)} {
            set xbuf [pin get dl_xbuf]
            puts [format "xbuf=%03o" $xbuf]
            pin set dl_xcsr 0200
            after 100
        }
    }
}
