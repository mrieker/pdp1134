
# simple test for lilmem.v
# ...and dma section of swlight.v

proc octal {n} {
    return [format "%06o" $n]
}

# simulator mode, halt processor, enable swlight, lilmem
pin set fpgamode 1
pin set sl_enable 1
pin set sl_haltreq 1
pin get sl_halted
pin set lm_enab 1

for {set pass 1} {$pass < 5} {incr pass} {

    # write random numbers to lilmem
    puts "pass $pass.A"
    for {set addr 0} {$addr < 4096} {incr addr 2} {
        set r [expr {int (rand () * 0200000)}]
        set rands($addr) $r
        pin set lm_addr $addr set lm_data $r
    }

    # readback lilmem and verify
    for {set addr 0} {$addr < 4096} {incr addr 2} {
        set r $rands($addr)
        set m [pin set lm_addr $addr get lm_data]
        if {$m != $r} {
            puts "lm [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }

    # readback via swlight.v and verify
    for {set addr 0} {$addr < 4096} {incr addr 2} {
        set r $rands($addr)
        set m [pin set sl_dmaaddr $addr sl_dmactrl 0 sl_dmastate 1 get sl_dmadata]
        if {$m != $r} {
            puts "sl [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }

    # write random numbers via swlight.v to lilmem
    puts "pass $pass.B"
    for {set addr 0} {$addr < 4096} {incr addr 2} {
        set r [expr {int (rand () * 0200000)}]
        set rands($addr) $r
        pin set sl_dmaaddr $addr sl_dmactrl 2 set sl_dmadata $r sl_dmastate 1
    }

    # readback lilmem and verify
    for {set addr 0} {$addr < 4096} {incr addr 2} {
        set r $rands($addr)
        set m [pin set lm_addr $addr get lm_data]
        if {$m != $r} {
            puts "lm [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }

    # readback via swlight.v and verify
    for {set addr 0} {$addr < 4096} {incr addr 2} {
        set r $rands($addr)
        set m [pin set sl_dmaaddr $addr sl_dmactrl 0 sl_dmastate 1 get sl_dmadata]
        if {$m != $r} {
            puts "sl [octal $addr] readback [octal $m] should be [octal $r]"
        }
    }
}
