#
#  Run on z11ctrl startup to define supplemental commands
#

proc helpini {} {
    puts ""
    puts "     dumpmem lo hi - dump memory from lo to hi address"
    puts "         flickcont - continue processing"
    puts "         flickinit - halt processor and initialize bus"
    puts "   flickstart addr - reset processor and start at given address"
    puts "         flickstep - step processor one instruction then print PC"
    puts "                     can be used as an halt if processor running"
    puts "         hardreset - hard reset processor to halted state"
    puts "           lockdma - lock access to dma controller"
    puts "           octal x - convert integer x to 6-digit octal string"
    puts "       rdbyte addr - read byte at given physical address"
    puts "       rdword addr - read word at given physical address"
    puts "  wrbyte addr data - write data byte to given physical address"
    puts "  wrword addr data - write data word to given physical address"
    puts "           unlkdma - unlock access to dma controller"
    puts ""
}

# dump memory
proc dumpmem {loaddr hiaddr} {
    for {set addr [expr {$loaddr & 0777740}]} {! [ctrlcflag] && ($addr <= $hiaddr)} {incr addr 040} {
        for {set j 0} {$j < 16} {incr j} {
            set data($j) [rdword [expr {$addr + $j * 2}]]
        }
        puts -nonewline " "
        for {set j 15} {$j >= 0} {incr j -1} {
            puts -nonewline [format " %06o" $data($j)]
        }
        puts [format " : %06o" $addr]
    }
}

# continue processing
proc flickcont {} {
    pin set sl_haltreq 0
}

# halt processor and initialize bus
proc flickinit {} {
    pin set sl_haltreq 1 sl_businit 1
    after 100
    if {! [pin get sl_halted]} {
        error "flickinit: processor failed to halt"
    }
    pin set sl_businit 0
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

# hard reset by asserting HLTRQ and strobing AC_LO,DC_LO
proc hardreset {} {
    pin set sl_haltreq 1    ;# so it halts when started back up
    pin set sl_acfail 1     ;# protocol is assert AC_LO
    after 5                 ;# wait at least 5mS
    pin set sl_dcfail 1     ;# then assert DC_LO
                            ;# swlight.v negates HLTRQ to processor
                            ;# processor asserts INIT
    after 10                ;# leave it resetting for 10mS
    pin set sl_dcfail 0     ;# dc power restored first
                            ;# swlight.v reasserts HLTRQ
    pin set sl_acfail 0     ;# ac power restored last
    for {set i 0} {! [pin get sl_halted]} {incr i} {
        if {$i > 100} {
            error "hardreset: processor not halted"
        }
        after 1             ;# give it a millisec to halt
    }
}

# lock acess to dma controller
proc lockdma {} {
    set lockedby [pin get sl_dmalock]
    set mypid [pid]
    if {$lockedby == $mypid} {error "dmalock: already locked by mypid $mypid"}
    while true {
        set lockedby [pin set sl_dmalock $mypid get sl_dmalock]
        if {$lockedby == $mypid} return
        if {! [file exists /proc/$lockedby]} {pin set sl_dmalock $lockedby}
        if [ctrlcflag] {error "lockdma: control-C waiting for pid $lockedby to release dmalock"}
    }
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
    lockdma
    pin set sl_dmaaddr [expr {$addr & 0777776}] sl_dmactrl 0 sl_dmastate 1
    set dmastate [pin get sl_dmastate]
    if {$dmastate != 0} {
        unlkdma
        error "rdbyte: dmastate stuck at $dmastate"
    }
    if {[pin get sl_dmafail]} {
        unlkdma
        error [format "rdbyte %06o timed out" $addr]
    }
    set data [pin get sl_dmadata]
    unlkdma
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
    lockdma
    pin set sl_dmaaddr $addr sl_dmactrl 0 sl_dmastate 1
    set dmastate [pin get sl_dmastate]
    if {$dmastate != 0} {
        unlkdma
        error "rdword: dmastate stuck at $dmastate"
    }
    if {[pin get sl_dmafail]} {
        unlkdma
        error [format "rdword %06o timed out" $addr]
    }
    set data [pin get sl_dmadata]
    unlkdma
    return $data
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
    lockdma
    pin set sl_dmaaddr [expr {$addr & 0777776}] sl_dmactrl 3 sl_dmadata $data sl_dmastate 1
    set dmastate [pin get sl_dmastate]
    if {$dmastate != 0} {
        unlkdma
        error "wrbyte: dmastate stuck at $dmastate"
    }
    if {[pin get sl_dmafail]} {
        unlkdma
        error [format "wrbyte %06o timed out" $addr]
    }
    unlkdma
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
    lockdma
    pin set sl_dmaaddr $addr sl_dmactrl 2 sl_dmadata $data sl_dmastate 1
    set dmastate [pin get sl_dmastate]
    if {$dmastate != 0} {
        unlkdma
        error "wrword: dmastate stuck at $dmastate"
    }
    if {[pin get sl_dmafail]} {
        unlkdma
        error [format "wrword %06o timed out" $addr]
    }
    unlkdma
}

# unlock acess to dma controller
proc unlkdma {} {
    set lockedby [pin get sl_dmalock]
    set mypid [pid]
    if {$lockedby != $mypid} {error "unlkdma: not locked by mypid $mypid"}
    pin set sl_dmalock $mypid
}

return "do 'helpini' to see commands provided by z11ctrlini.tcl"
