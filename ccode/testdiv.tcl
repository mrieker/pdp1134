
# test DIV instruction
#
#  $ ./z11ctrl
#  > pin set fpgamode 1 ;# 1 for sim; 2 for real
#  > source testdiv.tcl

hardreset
enabmem
wrword 024 0
wrword 026 0
hardreset
wrword 0100 071201  ;# DIV R1,R2
wrword 0102 000777
set n 0

while {! [ctrlcflag]} {

    ;# come up with random 16-bit signed divisor
    set divisor [expr {[randbits 16] - 0100000}]

    if {$divisor == 0} {

        ;# dividing by zero, use 32 random bits for dividend
        set dividend [randbits 32]
    } else {

        ;# come up with random 16-bit signed quotient
        set quotient [expr {[randbits 16] - 0100000}]

        ;# occasionally make it a 17-bit signed quotient for overflow testing
        if {[randbits 2] == 0} {
            set quotient [expr {$quotient * 2 + [randbits 1]}]
        }

        ;# compute what dividend should be without remainder
        set dividend [expr {$quotient * $divisor}]

        ;# come up with random remainder, same sign as dividend, magnitude less than divisor magnitude
        set divisabs [expr {($divisor < 0) ? - $divisor : $divisor}]
        set remaindr [expr {[randbits 16] % $divisabs}]
        if {$dividend < 0} {
            set remaindr [expr {- $remaindr}]
        }
        incr dividend $remaindr
    }

    ;# divisor  = some random divisor
    ;# quotient = some random quotient
    ;# reamindr = some random remainder
    ;# dividend = what the corresponding dividend is

    ;# puts "trying $dividend / $divisor => $quotient r $remaindr"

    ;# load up the registers including 4 random bits for NZVC
    set r1 [expr {$divisor & 0177777}]
    set r2 [expr {($dividend >> 16) & 0177777}]
    set r3 [expr {$dividend & 0177777}]
    set nzvc [randbits 4]

    wrword 0777701 $r1
    wrword 0777702 $r2
    wrword 0777703 $r3
    wrword 0777776 $nzvc

    ;# run the little program
    wrword 0777707 0100
    flickcont
    flickhalt
    set newpc [rdword 0777707]
    set newps [rdword 0777776]
    set newr2 [rdword 0777702]
    set newr3 [rdword 0777703]

    if {$newpc != 0102} {
        error [format "PC=%06o sb 000102" $newpc]
    }

    ;# what should be in output registers
    if {$divisor == 0} {
        ;# divide-by-zero overflow case
        ;#  both V and C
        set quo $r2
        set rem $r3
        set newnzvc 3
    } elseif {($quotient >= -32768) && ($quotient <= 32767)} {
        ;# quotient fits in 16-bit signed number, no overflow
        ;#  neither V nor C
        set quo [expr {$quotient & 0177777}]
        set rem [expr {$remaindr & 0177777}]
        set newnzvc [expr {(($quotient < 0) ? 8 : 0) | (($quotient == 0) ? 4 : 0)}]
    } else {
        ;# general overflow case
        ;#  just V bit, no C bit
        ;#  some cases 11/34 leaves original operands in registers
        ;#  some cases it leaves truncated results in registers
        ;#  so we just check the zero, overflow and carry bits
        set quo $newr2
        set rem $newr3
        set newnzvc [expr {($newps & 8) | 2}]
    }


    if {([incr n] % 1000 == 0) || ($divisor == 0)} {
        puts [format "%10u  R1=%06o R2R3=%06o.%06o=%011o PS=%02o => pdp R2=%06o R3=%06o PS=%02o  tcl R2=%06o R3=%06o PS=%02o" \
                $n $r1 $r2 $r3 $dividend $nzvc $newr2 $newr3 $newps $quo $rem $newnzvc]
    }
    if {($newr2 != $quo) || ($newr3 != $rem) || ($newps != $newnzvc)} {
        puts "...tried $dividend / $divisor => $quotient r $remaindr"
        error [format "%10u  R1=%06o R2R3=%06o.%06o=%011o PS=%02o => pdp R2=%06o R3=%06o PS=%02o  tcl R2=%06o R3=%06o PS=%02o" \
                $n $r1 $r2 $r3 $dividend $nzvc $newr2 $newr3 $newps $quo $rem $newnzvc]
    }
}
