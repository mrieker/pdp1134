
# use to test interrupts
# uses swlight sl_irqlev,sl_irqvec
# 1) first set sw_irqvec to vector 0000..0344 multiple of 4
# 2) then set sw_irqlev to level 4..7
# 3) pdp can clear sw_irqlev by clearing 777570

proc loadtest {} {
    pin set fpgamode 2
    pin set sl_enable 1
    pin set bm_enablo 0xFFFF

    hardreset

    wrword 01000 0005003    ;# clr  r3          clear interrupt flag
    wrword 01002 0005037    ;# clr  @#177570    clear sl_irqlev
    wrword 01004 0177570
    wrword 01006 0012706    ;# mov  #4000,sp    reset stack
    wrword 01010 0004000
    wrword 01012 0106403    ;# mtps r3          enable interrupts
    wrword 01014 0006203    ;# asr  r3          check then clear interrupt flag
    wrword 01016 0103376    ;# bcc  .-2         repeat if no interrupt
    wrword 01020 0010137    ;# mov  r1,@#5000   save vector that just interrupted
    wrword 01022 0005000
    wrword 01024 0005237    ;# inc  @#5002      inc total number of interrupts
    wrword 01026 0005002
    wrword 01030 0000764    ;# br   1002        repeat for another

    # fill vectors to put vector number in R1, increment R3, then leave interrupts disabled
    for {set v 0} {$v < 256} {incr v 4} {
        wrword $v [expr {$v * 2 + 02000}]
        wrword [expr {$v + 2}] 0340
        wrword [expr {$v * 2 + 02000}] 0012701  ;# mov  #v,r1
        wrword [expr {$v * 2 + 02002}] $v
        wrword [expr {$v * 2 + 02004}] 0005203  ;# inc  r3
        wrword [expr {$v * 2 + 02006}] 0000207  ;# rts  pc
    }
}

proc runtest {} {
    hardreset
    wrword 05000 0000000    ;# latest interrupt vector
    wrword 05002 0000000    ;# total number interrupts
    flickstart 01000

    puts "checking for stray"
    for {set i 0} {$i < 10000} {incr i} {
        set n [rdword 05002]
        if {$n > 0} {
            error [format "initial stray interrupt %03o (sl_irqvec %03o, sl_irqlev %d)" [rdword 05000] [pin get sl_irqvec] [pin get sl_irqlev]]
        }
    }

    puts "pounding away"
    set num 0
    while {! [ctrlcflag]} {
        set lev [expr {[randbits 2] + 4}]
        set vec [expr {[randbits 6] * 4}]
        if {$vec == 0} continue
        pin set sl_irqvec $vec sl_irqlev $lev
        for {set i 0} {[pin get sl_irqlev] != 0} {incr i} {
            if {$i > 1000000} {
                error "timed out lev=$lev vec=$vec"
            }
        }
        set rbvec [rdword 05000]
        set rbnum [rdword 05002]
        set num [expr {($num + 1) & 65535}]
        if {($rbnum != $num) || ($rbvec != $vec)} {
            error [format "mismatch is %03o %6d, should be %03o %6d" $rbvec $rbnum $vec $num]
        }
        if {$num % 1000 == 0} {
            puts -nonewline "*"
            flush stdout
        }
    }
    puts ""
}

puts ""
puts "  loadtest  - reset processor then load test program in memory"
puts "  runtest   - run the test program until control-C"
puts ""
