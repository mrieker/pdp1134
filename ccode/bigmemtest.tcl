
# simple test for bigmem.v
# ...and dma section of ky11.v

# halt processor, enable bigmem
set fpgamode [pin fpgamode]
if {($fpgamode != 1) && ($fpgamode != 2)} {
    puts "retry with sim or real mode (pin set fpgamode 1 or 2)"
    return
}
hardreset
pin set bm_enablo 0xFFFFFFFF bm_enabhi 0x3FFFFFFF

set loaddr 0
set hiaddr 0757777

for {set pass 1} {$pass <= 5} {incr pass} {

    # write random numbers to bigmem
    puts "pass $pass.A"
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r [randbits 16]
        set rands($addr) $r
        bmwrword $addr $r
    }

    # readback bigmem and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [bmrdword $addr]
        if {$m != $r} {
            puts "bm [octal $addr] readback [octal $m] should be [octal $r]"
            return
        }
    }

    # readback via ky11.v and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [rdword $addr]
        if {$m != $r} {
            puts "ky [octal $addr] readback [octal $m] should be [octal $r]"
            return
        }
    }

    # write random numbers via ky11.v to bigmem
    puts "pass $pass.B"
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r [randbits 16]
        set rands($addr) $r
        wrword $addr $r
    }

    # readback bigmem and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [bmrdword $addr]
        if {$m != $r} {
            puts "bm [octal $addr] readback [octal $m] should be [octal $r]"
            return
        }
    }

    # readback via ky11.v and verify
    for {set addr $loaddr} {$addr <= $hiaddr} {incr addr 2} {
        set r $rands($addr)
        set m [rdword $addr]
        if {$m != $r} {
            puts "ky [octal $addr] readback [octal $m] should be [octal $r]"
            return
        }
    }

    # test cpu general registers
    puts "pass $pass.C"
    for {set reg 0} {$reg < 8} {incr reg} {
        set r [randbits 16]
        set rands($reg) $r
        wrword 077770$reg $r
        if {[pin get ky_dmatimo]} {
            puts "ky write R$reg timed out"
            return
        }
    }
    for {set reg 0} {$reg < 8} {incr reg} {
        set r $rands($reg)
        set m [rdword 077770$reg]
        if {[pin get ky_dmatimo]} {
            puts "ky read R$reg timed out"
            return
        } elseif {$m != $r} {
            puts "ky R$reg readback [octal $m] should be [octal $r]"
        }
    }
}
