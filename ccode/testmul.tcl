
# test MUL instruction
#
#  $ ./z11ctrl
#  > pin set fpgamode 1 ;# 1 for sim; 2 for real
#  > source testmul.tcl

hardreset
enabmem
wrword 024 0
wrword 026 0
hardreset
wrword 0100 070200  ;# MUL R0,R2
wrword 0102 000777
set n 0

while {! [ctrlcflag]} {
    if {[randbits 2] == 0} {
        set r0 [expr {([randbits 8] - 0200) & 0177777}]
        set r2 [expr {([randbits 8] - 0200) & 0177777}]
    } else {
        set r0 [randbits 16]
        set r2 [randbits 16]
    }
    set nzvc [randbits 4]
    wrword 0777700 $r0
    wrword 0777702 $r2
    wrword 0777776 $nzvc
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

    set sgnr0 [expr {($r0 ^ 0100000) - 0100000}]
    set sgnr2 [expr {($r2 ^ 0100000) - 0100000}]
    set prod  [expr {$sgnr0 * $sgnr2}]
    set newnzvc [expr {(($prod < 0) ? 8 : 0) | (($prod == 0) ? 4 : 0) | ($prod < -32768) | ($prod > 32767)}]

    set sgnr2r3 [expr {((($newr2 << 16) | $newr3) ^ 020000000000) - 020000000000}]

    if {[incr n] % 1000 == 0} {
        puts [format "%10u  R0=%06o R2=%06o PS=%02o => R2R3=%011o=%06o.%06o PS=%02o sb R2R3=%011o PS=%02o" \
                $n $r0 $r2 $nzvc $sgnr2r3 $newr2 $newr3 $newps $prod $newnzvc]
    }
    if {($sgnr2r3 != $prod) || ($newps != $newnzvc)} {
        error [format "%10u  R0=%06o R2=%06o PS=%02o => R2R3=%011o=%06o.%06o PS=%02o sb R2R3=%011o PS=%02o" \
            $n $r0 $r2 $nzvc $sgnr2r3 $newr2 $newr3 $newps $prod $newnzvc]
    }
}
