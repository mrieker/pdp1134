#
#  Run on z11ctrl startup to define supplemental commands
#

proc helpini {} {
    puts ""
    puts "         flickcont - continue processing"
    puts "         flickinit - halt processor and initialize bus"
    puts "   flickstart addr - reset processor and start at given address"
    puts "         flickstep - step processor one instruction then print PC"
    puts "                     can be used as an halt if processor running"
    puts "           octal x - convert integer x to 6-digit octal string"
    puts "       rdbyte addr - read byte at given physical address"
    puts "       rdword addr - read word at given physical address"
    puts "  wrbyte addr data - write data byte to given physical address"
    puts "  wrword addr data - write data word to given physical address"
    puts ""
}

# continue processing
proc flickcont {} {
    pin set sl_haltreq 0
}

# halt processor and initialize bus
proc flickinit {} {
    pin set sl_haltreq 1 sl_businit 1 sl_businit 0
}

# start processor at given address
proc flickstart {addr} {
    flickinit
    wrword 0777707 $addr
    flickcont
}

# single step one instruction then print PC
# - can also be used as an halt
proc flickstep {} {
    pin set sl_stepreq 1
    if {! [pin get sl_halted]} {
        error "flickstep: processor did not halt after step"
    }
    puts "PC=[octal [rdword 0777707]]"
}

# convert given number to 6-digit octal string
proc octal {x} {
    return [format "%06o" $x]
}

# read a byte from unibus via dma (swlight.v)
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

# read a word from unibus via dma (swlight.v)
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

# write a byte to unibus via dma (swlight.v)
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

# write a word to unibus via dma (swlight.v)
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

return "do 'helpini' to see commands provided by z11ctrlini.tcl"
