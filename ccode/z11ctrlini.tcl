#
#  Run on z11ctrl startup to define supplemental commands
#

proc helpini {} {
    puts ""
    puts "       bmrdbyte addr - read byte from fpga memory"
    puts "       bmrdword addr - read word from fpga memory"
    puts "  bmwrbyte addr data - write byte to fpga memory"
    puts "  bmwrword addr data - write word to fpga memory"
    puts "       dumpmem lo hi - dump memory from lo to hi address"
    puts "           flickcont - continue processing"
    puts "  flickstart pc [ps] - reset processor and start at given address"
    puts "           flickstep - step processor one instruction then print PC"
    puts "                       can be used as an halt if processor running"
    puts "           hardreset - hard reset processor to halted state"
    puts "             lockdma - lock access to dma controller"
    puts "             octal x - convert integer x to 6-digit octal string"
    puts "         rdbyte addr - read byte at given physical address"
    puts "         rdword addr - read word at given physical address"
    puts "    wrbyte addr data - write data byte to given physical address"
    puts "    wrword addr data - write data word to given physical address"
    puts "             unlkdma - unlock access to dma controller"
    puts ""
}

# read byte directly from bigmem (no dma cycle)
proc bmrdbyte {addr} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "bmrdbyte: bad address %o" $addr]
    }
    pin set bm_armaddr $addr bm_armfunc 4
    for {set i 0} {[set x [pin bm_armfunc]] != 0} {incr i} {
        if {$i > 1000} {
            error "bmrdbyte: stuck at $x"
        }
    }
    set data [pin bm_armdata]
    return [expr {($addr & 1) ? ($data >> 8) : ($data & 255)}]
}

# read word directly from bigmem (no dma cycle)
proc bmrdword {addr} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "bmrdword: bad address %o" $addr]
    }
    if {$addr & 1} {
        error [format "bmrdword: odd address %06o" $addr]
    }
    pin set bm_armaddr $addr bm_armfunc 4
    for {set i 0} {[set x [pin bm_armfunc]] != 0} {incr i} {
        if {$i > 1000} {
            error "bmrdword: stuck at $x"
        }
    }
    return [pin bm_armdata]
}

# write byte directly to bigmem (no dma cycle)
proc bmwrbyte {addr data} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "bmwrbyte: bad address %o" $addr]
    }
    if {($data < 0) || ($data > 0377)} {
        error [format "bmwrbyte: bad data %o" $data]
    }
    pin set bm_armaddr $addr bm_armdata [expr {$data*0401}] bm_armfunc 1
    for {set i 0} {[set x [pin bm_armfunc]] != 0} {incr i} {
        if {$i > 1000} {
            error "bmwrbyte: stuck at $x"
        }
    }
}

# write word directly to bigmem (no dma cycle)
proc bmwrword {addr data} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "bmwrword: bad address %o" $addr]
    }
    if {$addr & 1} {
        error [format "bmwrword: odd address %06o" $addr]
    }
    if {($data < 0) || ($data > 0177777)} {
        error [format "bmwrword: bad data %o" $data]
    }
    pin set bm_armaddr $addr bm_armdata $data bm_armfunc 3
    for {set i 0} {[set x [pin bm_armfunc]] != 0} {incr i} {
        if {$i > 1000} {
            error "bmwrword: stuck at $x"
        }
    }
}

# dump memory
proc dumpmem {loaddr hiaddr {rwfunc rdword}} {
    set loeven [expr {$loaddr & -2}]
    for {set addr [expr {$loaddr & -040}]} {! [ctrlcflag] && ($addr <= $hiaddr)} {incr addr 040} {
        for {set j 15} {$j >= 0} {incr j -1} {
            set a [expr {$addr + $j * 2}]
            if {($a >= $loeven) && ($a <= $hiaddr)} {
                set data [$rwfunc $a]
                if {$a < $loaddr} {
                    puts -nonewline [format " %03o   " [expr {$data >> 8}]]
                } elseif {$a == $hiaddr} {
                    puts -nonewline [format "    %03o" [expr {$data & 255}]]
                } else {
                    puts -nonewline [format " %06o" $data]
                }
                set bytes([expr {$j*2}]) [expr {$data & 255}]
                set bytes([expr {$j*2+1}]) [expr {$data >> 8}]
            } else {
                puts -nonewline "       "
            }
        }
        puts -nonewline [format " : %06o : " $addr]
        for {set j 0} {$j <= 31} {incr j} {
            set a [expr {$addr + $j}]
            if {$a > $hiaddr} break
            if {$a < $loaddr} {
                puts -nonewline " "
            } else {
                if {($a == $loaddr) || ($j == 0)} {
                    puts -nonewline "<"
                }
                set byte [expr {$bytes($j) & 127}]
                if {($byte < 32) || ($byte > 126)} {
                    puts -nonewline "."
                } else {
                    puts -nonewline [format "%c" $byte]
                }
            }
        }
        puts ">"
    }
}

# continue processing
proc flickcont {} {
    pin set sl_haltreq 0
}

# start processor at given address
proc flickstart {pc {ps 0340}} {
    hardreset
    wrword 0777707 $pc
    wrword 0777776 $ps
    flickcont
}

# single step one instruction then return PC
# - can also be used as an halt
proc flickstep {} {
    pin set sl_stepreq 1
    for {set i 0} {[pin sl_stepreq] || ! [pin sl_halted]} {incr i} {
        if {$i > 1000} {
            error "flickstep: processor did not step"
        }
    }
    return [rdword 0777707]
}

# hard reset by asserting HLTRQ and strobing AC_LO,DC_LO
# waits for processor to halt after the reset
# it theoretically has read the 024/026 power-up vector into PC/PS
proc hardreset {} {
    pin set sl_haltreq 1    ;# so it halts when started back up
    pin set man_ac_lo_out_h 1 man_dc_lo_out_h 1
    after 200
    pin set man_dc_lo_out_h 0 man_ac_lo_out_h 0
    for {set i 0} true {incr i} {
        after 1
        if {[pin sl_halted]} break
        if {$i > 1000} {
            error "hardreset: processor did not halt"
        }
    }
}

# lock acess to dma controller
proc lockdma {} {
    set lockedby [pin sl_dmalock]
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
    for {set i 0} {[set dmastate [pin sl_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "rdbyte: dmastate stuck at $dmastate"
        }
    }
    if {[pin sl_dmafail]} {
        unlkdma
        error [format "rdbyte %06o timed out" $addr]
    }
    set data [pin sl_dmadata]
    unlkdma
    if {$addr & 1} {set data [expr {$data >> 8}]}
    return [expr {$data & 0377}]
}

# read a word from unibus via dma (swlight.v)
proc rdword {addr} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "rdword: bad address %o" $addr]
    }
    if {($addr & 1) && (($addr < 0777700) || ($addr > 0777717))} {
        error [format "rdword: odd address %06o" $addr]
    }
    lockdma
    pin set sl_dmaaddr $addr sl_dmactrl 0 sl_dmastate 1
    for {set i 0} {[set dmastate [pin sl_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "rdword: dmastate stuck at $dmastate"
        }
    }
    if {[pin sl_dmafail]} {
        unlkdma
        error [format "rdword %06o timed out" $addr]
    }
    set data [pin sl_dmadata]
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
    for {set i 0} {[set dmastate [pin sl_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "wrbyte: dmastate stuck at $dmastate"
        }
    }
    if {[pin sl_dmafail]} {
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
    for {set i 0} {[set dmastate [pin sl_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "wrword: dmastate stuck at $dmastate"
        }
    }
    if {[pin sl_dmafail]} {
        unlkdma
        error [format "wrword %06o timed out" $addr]
    }
    unlkdma
}

# unlock acess to dma controller
proc unlkdma {} {
    set lockedby [pin sl_dmalock]
    set mypid [pid]
    if {$lockedby != $mypid} {error "unlkdma: not locked by mypid $mypid"}
    pin set sl_dmalock $mypid
}

return "do 'helpini' to see commands provided by z11ctrlini.tcl"
