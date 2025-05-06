
# simple test for bigmem.v
# ...and dma section of swlight.v

proc octal {n} {
    return [format "%06o" $n]
}

# simulator mode, halt processor, enable swlight, bigmem
pin set fpgamode 1
pin set sl_enable 1
pin set sl_haltreq 1
pin get sl_halted
pin set bm_enablo 0xFFFFFFFF bm_enabhi 0x3FFFFFFF

set loaddr 0
set hiaddr 0757777

for {set pass 1} {$pass <= 5} {incr pass} {

    # write random numbers to bigmem
    puts "pass $pass.A"
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r [expr {int (rand () * 0200000)}]
        set rands($addr) $r
        pin set bm_armaddr $addr bm_armdata $r bm_armfunc 3
    }

    # readback bigmem and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [pin set bm_armaddr $addr bm_armfunc 4 get bm_armdata]
        if {$m != $r} {
            puts "bm [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }

    # readback via swlight.v and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [pin set sl_dmaaddr $addr sl_dmactrl 0 sl_dmastate 1 get sl_dmadata]
        if {$m != $r} {
            puts "sl [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }

    # write random numbers via swlight.v to bigmem
    puts "pass $pass.B"
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r [expr {int (rand () * 0200000)}]
        set rands($addr) $r
        pin set sl_dmaaddr $addr sl_dmactrl 2 set sl_dmadata $r sl_dmastate 1
    }

    # readback bigmem and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [pin set bm_armaddr $addr bm_armfunc 4 get bm_armdata]
        if {$m != $r} {
            puts "bm [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }

    # readback via swlight.v and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [pin set sl_dmaaddr $addr sl_dmactrl 0 sl_dmastate 1 get sl_dmadata]
        if {$m != $r} {
            puts "sl [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }

    # test cpu general registers
    puts "pass $pass.C"
    for {set reg 0} {$reg < 8} {incr reg} {
        set r [expr {int (rand () * 0200000)}]
        set rands($reg) $r
        pin set sl_dmaaddr 077770$reg sl_dmactrl 2 set sl_dmadata $r sl_dmastate 1
        if {[pin get sl_dmafail]} {
            puts "sl write R$reg failed"
        }
    }
    for {set reg 0} {$reg < 8} {incr reg} {
        set r $rands($reg)
        set m [pin set sl_dmaaddr 077770$reg sl_dmactrl 0 sl_dmastate 1 get sl_dmadata]
        if {[pin get sl_dmafail]} {
            puts "sl read R$reg failed"
        } elseif {$m != $r} {
            puts "sl R$reg readback [octal $m] should be [octal $r]"
        }
    }
}
