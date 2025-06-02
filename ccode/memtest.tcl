
# simple test for external memory
# ...and dma section of ky11.v

proc myrdword {addr} {
    return [rdword $addr]

    pin set man_a_out_h $addr
    pin set man_c_out_h 0
    pin set man_msyn_out_h 1
    if {! [pin get dev_ssyn_h]} {
        error "myrdword: ssyn failed"
    }
    set data [pin get dmx_d_out_h]
    pin set man_msyn_out_h 0
    pin set man_a_out_h 0
    return $data
}

proc mywrword {addr data} {
    wrword $addr $data
    return

    pin set man_a_out_h $addr
    pin set man_c_out_h 2
    pin set man_d_out_h $data
    pin set man_msyn_out_h 1
    if {! [pin get dev_ssyn_h]} {
        error "mywrword: ssyn failed"
    }
    pin set man_msyn_out_h 0
    pin set man_a_out_h 0
    pin set man_c_out_h 0
    pin set man_d_out_h 0
}

#######################
## SCRIPT START HERE ##
#######################

# real mode
##pin set fpgamode 2

# address range
set loaddr 0
set hiaddr 07777

proc memtest {{loaddr 0} {hiaddr 07777}} {

    for {set pass 1} {! [ctrlcflag] && ($pass <= 5)} {incr pass} {

        for {set mode 0} {! [ctrlcflag] && ($mode < 3)} {incr mode} {

            # write random numbers via ky11.v
            puts "pass $pass.$mode"
            for {set addr $loaddr} {! [ctrlcflag] && ($addr <= $hiaddr)} {incr addr 2} {
                switch $mode {
                    0 {set r 0}
                    1 {set r 0177777}
                    2 {set r [randbits 16]}
                }
                set rands($addr) $r
                mywrword $addr $r
            }

            # readback via ky11.v and verify
            for {set addr $loaddr} {! [ctrlcflag] && ($addr <= $hiaddr)} {incr addr 2} {
                set r $rands($addr)
                set m [myrdword $addr]
                if {$m != $r} {
                    puts "[octal $addr] readback [octal $m] should be [octal $r]"
                    return
                }
            }
        }

        # test cpu general registers
        puts "pass $pass.R"
        for {set reg 0} {$reg < 8} {incr reg} {
            set r [randbits 16]
            set rands($reg) $r
            mywrword 077770$reg $r
        }
        for {set reg 0} {$reg < 8} {incr reg} {
            set r $rands($reg)
            set m [myrdword 077770$reg]
            if {$m != $r} {
                puts "R$reg readback [octal $m] should be [octal $r]"
                return
            }
        }
    }
}

proc testmuxbits {} {
    for {set j 0} {$j < 3} {incr j} {
        set letter [string index "acd" $j]
        set nbits [expr {($j == 0) ? 18 : ($j == 1) ? 2 : 16}]
        puts "$letter bits one bit at a time"
        for {set i 0} {$i < $nbits} {incr i} {
            set mask [expr {1 << $i}]
            pin set man_${letter}_out_h $mask
            set got [pin get dev_${letter}_h]
            if {$got == $mask} {
                puts "$letter bit $i ok"
            } else {
                puts "$letter bit $i bad - got [octal $got]"
            }
        }
        set nloops [expr {2 << $nbits}]
        puts "$letter random numbers ($nloops)"
        for {set i 0} {! [ctrlcflag] && ($i < $nloops)} {incr i} {
            set ran [randbits $nbits]
            set got [pin set man_${letter}_out_h $ran get dev_${letter}_h]
            if {$got != $ran} {
                puts "$letter set [octal $ran] got [octal $got]"
                return
            }
            if {$i % 10000 == 0} {puts "$letter $i"}
        }
        pin set man_${letter}_out_h 0
    }
}

puts ""
puts "  pin set fpgamode 1                        - simulator - ignore unibus"
puts "  pin set fpgamode 2                        - use real unibus"
puts "  pin set bm_enabhi 0 bm_enablo 0           - disable fpga memory"
puts "  pin set bm_enabhi 0 bm_enablo 0xFFFF      - enable fpga memory 000000..177777"
puts "  pin set bm_enabhi 0x3FFF0000 bm_enablo 0  - enable fpga memory 600000..757777"
puts "  memtest <loaddr> <hiaddr>                 - test range of memory"
puts "                                              default 0 07777"
puts "  testmuxbits                               - test address,control,data bit lines"
puts ""
