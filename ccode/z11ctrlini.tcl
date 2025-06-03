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
    puts "        enabmem size - enable given size of memory"
    puts "           flickcont - continue processing"
    puts "  flickstart pc \[ps\] - reset processor and start at given address"
    puts "           flickstep - step processor one instruction then print PC"
    puts "                       can be used as an halt if processor running"
    puts "           hardreset - hard reset processor to halted state"
    puts "             loadbin - load binary tape file, return start address"
    puts "             loadlst - load from MACRO11 listing"
    puts "             lockdma - lock access to dma controller"
    puts "             octal x - convert integer x to 6-digit octal string"
    puts "         rdbyte addr - read byte at given physical address"
    puts "         rdword addr - read word at given physical address"
    puts "             realmem - determine size of real memory"
    puts "           steptrace - single step with disassembly"
    puts "       steptraceloop - single step with disassembly - looped"
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
    set pem [expr {1 + ($addr & 1)}]
    if {[pin bm_armperr] & $pem} {
        error [format "bmrdbyte: parity error at %06o" $addr]
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
    if {[pin bm_armperr] & 3} {
        error [format "bmrdword: parity error at %06o" $addr]
    }
    return [pin bm_armdata]
}

# write byte directly to bigmem (no dma cycle)
#  input:
#   addr = 18-bit address to write to
#   data = 8-bit data to write
#   pe = 0 : write correct parity to location
#        1 : write incorrect parity to location
proc bmwrbyte {addr data {pe 0}} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "bmwrbyte: bad address %o" $addr]
    }
    if {($data < 0) || ($data > 0377)} {
        error [format "bmwrbyte: bad data %o" $data]
    }
    set data [expr {$data * 0401}]
    set pem  [expr {$pe * 3}]
    set func [expr {1 + ($addr & 1)}]
    pin set bm_armaddr $addr bm_armdata $data bm_armperr $pem bm_armfunc $func
    for {set i 0} {[set x [pin bm_armfunc]] != 0} {incr i} {
        if {$i > 1000} {
            error "bmwrbyte: stuck at $x"
        }
    }
}

# write word directly to bigmem (no dma cycle)
#  input:
#   addr = 18-bit address to write to
#   data = 16-bit data to write
#   pe = 0 : write correct parity to location
#        1 : write incorrect parity to location
proc bmwrword {addr data {pe 0}} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "bmwrword: bad address %o" $addr]
    }
    if {$addr & 1} {
        error [format "bmwrword: odd address %06o" $addr]
    }
    if {($data < 0) || ($data > 0177777)} {
        error [format "bmwrword: bad data %o" $data]
    }
    set pem [expr {$pe * 3}]
    pin set bm_armaddr $addr bm_armdata $data bm_armperr $pem bm_armfunc 3
    for {set i 0} {[set x [pin bm_armfunc]] != 0} {incr i} {
        if {$i > 1000} {
            error "bmwrword: stuck at $x"
        }
    }
}

# dump memory
proc dumpmem {loaddr hiaddr {rwfunc rdword}} {
    if {($loaddr < 0) || ($hiaddr > 0777777) || ($hiaddr < $loaddr)} {
        error "dumpmem: addresses $loaddr $hiaddr out of range or order"
    }
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

# enable memory to the given size
# uses any available real memory then fills with fpga (bigmem.v) after that
proc enabmem {{size 0760000}} {
    if {$size > 0760000} {set size 0760000}
    # see how much real memory there is
    set realsize [realmem]
    # one bit in enablo per 4KB block 000000..377777
    set enablo 0
    # one bit in enabhi per 4KB block 400000..757777
    set enabhi 0
    # if less than wanted, enable some bigmem.v memory to fill in
    while {$realsize < $size} {
        if {$realsize < 0400000} {
            incr enablo [expr (1 << ($realsize / 010000))]
        } else {
            incr enabhi [expr (1 << (($realsize - 0400000) / 010000))]
        }
        incr realsize 010000
    }
    # turn on needed bigmem.v pages
    pin set bm_enablo $enablo bm_enabhi $enabhi
}

# continue processing
proc flickcont {} {
    pin set ky_haltreq 0
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
    pin set ky_stepreq 1
    for {set i 0} {[pin ky_stepreq] || ! [pin ky_halted]} {incr i} {
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
    pin set ky_haltreq 1    ;# so it halts when started back up
    pin set man_ac_lo_out_h 1 man_dc_lo_out_h 1
    after 200
    pin set man_dc_lo_out_h 0 man_ac_lo_out_h 0
    for {set i 0} true {incr i} {
        after 1
        if {[pin ky_halted]} break
        if {$i > 1000} {
            error "hardreset: processor did not halt"
        }
    }
}

# load binary tape file
# returns start address
proc loadbin {binname} {
    set binfile [open $binname]
    fconfigure $binfile -translation binary
    set state "lolead"
    while {$state != "done"} {
        set ch [read $binfile 1]
        if {$ch == ""} {
            error "no termination block"
        }
        scan $ch "%c" by
        switch $state {
            "lolead" {
                if {$by == 1} {
                    set state "hilead"
                    set cksum 1
                } elseif {$by != 0} {
                    error "bad leader byte $by"
                }
            }
            "hilead" {
                if {$by != 0} {
                    error "bad 2nd leader byte $by"
                }
                set state "losize"
            }
            "losize" {
                incr cksum $by
                set size $by
                set state "hisize"
            }
            "hisize" {
                incr cksum $by
                incr size [expr {$by << 8}]
                if {$size < 6} {
                    error "bad size $size"
                }
                incr size -6
                set state "loaddr"
            }
            "loaddr" {
                incr cksum $by
                set addr $by
                set state "hiaddr"
            }
            "hiaddr" {
                incr cksum $by
                incr addr [expr {$by << 8}]
                puts [format "loadbin: loading %06o at %06o" $size $addr]
                set state [expr {($size == 0) ? "end" : "data"}]
            }
            "data" {
                incr cksum $by
                if {$size > 0} {
                    wrbyte $addr $by
                    incr addr
                    incr size -1
                } elseif {($cksum & 255) != 0} {
                    error "bad checksum $cksum ending at $addr"
                } else {
                    set state "lolead"
                }
            }
            "end" {
                incr cksum $by
                if {($cksum & 255) != 0} {
                    error "bad checksum $cksum ending at $addr"
                }
                set state "done"
            }
        }
    }
    close $binfile
    return $addr
}

# load from MACRO11 listing
proc loadlst {lstname} {
    set lstfile [open $lstname]
    while {[gets $lstfile lstline] >= 0} {
        set addr 0[string trim [string range $lstline 8 15]]
        if {[string length $addr] == 7} {
            for {set i 15} {$i < 38} {incr i 8} {
                set digits 0[string trim [string range $lstline $i [expr {$i + 7}]]]
                switch [string length $digits] {
                    1 { }
                    4 {
                        wrbyte $addr $digits
                        incr addr 1
                    }
                    7 {
                        wrword $addr $digits
                        incr addr 2
                    }
                    default {
                        error "bad data $digits at addr [octal $addr]"
                    }
                }
            }
        }
    }
    close $lstfile
}

# lock acess to dma controller
proc lockdma {} {
    set lockedby [pin ky_dmalock]
    set mypid [pid]
    if {$lockedby == $mypid} {error "dmalock: already locked by mypid $mypid"}
    while true {
        set lockedby [pin set ky_dmalock $mypid get ky_dmalock]
        if {$lockedby == $mypid} return
        if {! [file exists /proc/$lockedby]} {pin set ky_dmalock $lockedby}
        if [ctrlcflag] {error "lockdma: control-C waiting for pid $lockedby to release dmalock"}
    }
}

# convert given number to 6-digit octal string
proc octal {x} {
    return [format "%06o" $x]
}

# read a byte from unibus via dma (ky11.v)
proc rdbyte {addr} {
    if {($addr < 0) || ($addr > 0777777)} {
        error "rdbyte: bad address $addr"
        error [format "rdbyte: bad address %o" $addr]
    }
    lockdma
    pin set ky_dmaaddr [expr {$addr & 0777776}] ky_dmactrl 0 ky_dmastate 1
    for {set i 0} {[set dmastate [pin ky_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "rdbyte: dmastate stuck at $dmastate"
        }
    }
    if {[pin ky_dmatimo]} {
        unlkdma
        error [format "rdbyte %06o timed out" $addr]
    }
    if {[pin ky_dmaperr]} {
        unlkdma
        error [format "rdbyte %06o parity error" $addr]
    }
    set data [pin ky_dmadata]
    unlkdma
    if {$addr & 1} {set data [expr {$data >> 8}]}
    return [expr {$data & 0377}]
}

# read a word from unibus via dma (ky11.v)
proc rdword {addr} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "rdword: bad address %o" $addr]
    }
    if {($addr & 1) && (($addr < 0777700) || ($addr > 0777717))} {
        error [format "rdword: odd address %06o" $addr]
    }
    lockdma
    pin set ky_dmaaddr $addr ky_dmactrl 0 ky_dmastate 1
    for {set i 0} {[set dmastate [pin ky_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "rdword: dmastate stuck at $dmastate"
        }
    }
    if {[pin ky_dmatimo]} {
        unlkdma
        error [format "rdword %06o timed out" $addr]
    }
    if {[pin ky_dmaperr]} {
        unlkdma
        error [format "rdword %06o parity error" $addr]
    }
    set data [pin ky_dmadata]
    unlkdma
    return $data
}

# measure how much real memory is on the system
# reads starting at 000000 up to 0760000 in 4KB increments
# ...until a bus timeout occurs
proc realmem {} {
    # make sure nothing else futzing with ky_dma... pins
    lockdma
    # turn off all bigmem.v memory pages
    pin set bm_enablo 0 bm_enabhi 0
    # loop through addresses, 4KB at a time
    for {set addr 0} {$addr < 0760000} {incr addr 01000} {
        # tell ky11.v to try to read word from $addr
        pin set ky_dmaaddr $addr ky_dmactrl 0 ky_dmastate 1
        # it should always complete the cycle, one way or the other
        for {set i 0} {[set dmastate [pin ky_dmastate]] != 0} {incr i} {
            if {$i > 1000} {
                unlkdma
                error "realmem: dmastate stuck at $dmastate"
            }
        }
        # if timeout, reached end of memory
        if {[pin ky_dmatimo]} break
    }
    # tell others we're done with ky_dma... pins
    unlkdma
    # return size found
    return $addr
}

# step, printing disassembly
proc steptrace {} {
    if {! [pin ky_halted]} {
        error "steptrace: processor must be halted"
    }
    set pc  [rdword 0777707]
    set dis [disasop]
    set dis [string range $dis 1 end]
    puts [format "%06o %s" $pc $dis]
    return [flickstep]
}

# step, printing disassembly, in a loop
# stop on control-C or HALT instruction
proc steptraceloop {} {
    while {! [ctrlcflag] && ! [pin ky_haltins]} steptrace
}

# write a byte to unibus via dma (ky11.v)
proc wrbyte {addr data} {
    if {($addr < 0) || ($addr > 0777777)} {
        error [format "wrbyte: bad address %o" $addr]
    }
    if {($data < 0) || ($data > 0377)} {
        error [format "wrbyte: bad data %o" $data]
    }
    set data [expr {$data * 0401}]
    lockdma
    pin set ky_dmaaddr $addr ky_dmactrl 3 ky_dmadata $data ky_dmastate 1
    for {set i 0} {[set dmastate [pin ky_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "wrbyte: dmastate stuck at $dmastate"
        }
    }
    if {[pin ky_dmatimo]} {
        unlkdma
        error [format "wrbyte %06o timed out" $addr]
    }
    unlkdma
}

# write a word to unibus via dma (ky11.v)
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
    pin set ky_dmaaddr $addr ky_dmactrl 2 ky_dmadata $data ky_dmastate 1
    for {set i 0} {[set dmastate [pin ky_dmastate]] != 0} {incr i} {
        if {$i > 1000} {
            unlkdma
            error "wrword: dmastate stuck at $dmastate"
        }
    }
    if {[pin ky_dmatimo]} {
        unlkdma
        error [format "wrword %06o timed out" $addr]
    }
    unlkdma
}

# unlock acess to dma controller
proc unlkdma {} {
    set lockedby [pin ky_dmalock]
    set mypid [pid]
    if {$lockedby != $mypid} {error "unlkdma: not locked by mypid $mypid"}
    pin set ky_dmalock $mypid
}

return "do 'helpini' to see commands provided by z11ctrlini.tcl"
