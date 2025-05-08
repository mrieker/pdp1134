
proc rlread {drive block addr {wcnt 128}} {
    rlseek $drive $block
    set cyl [expr {int ($block / 80)}]
    set trk [expr {($block % 80) >= 40}]
    set sec [expr {$block % 40}]
    wrword 0774406 [expr {0200000 - $wcnt}]
    wrword 0774404 [expr {($cyl << 7) | ($trk << 6) | $sec}]
    wrword 0774402 [expr {$addr & 0177777}]
    wrword 0774400 [expr {($drive << 8) | (($addr >> 16) << 4) | 12}]
    for {set i 0} true {incr i} {
        set csr [rdword 0774400]
        if {$csr & 0x80} break
        if {$i > 1000000} {error [format "rlread: failed to see RDY in RLCS %04X reading block" $csr]}
        if [ctrlcflag] return
    }
    if {$csr & 0x8000} {
        error [format "rlread: error RLCS %04X reading block $block" $csr]
    }
}

proc rlseek {drive block} {

    # get drive status
    set gsdar 003
    while true {
        wrword 0774404 $gsdar                       ;# set DAR = 000003 | clear vc
        wrword 0774400 [expr {($drive << 8) | 4}]   ;# set CSR = 000004 | driveselect
        for {set i 0} true {incr i} {
            set csr [rdword 0774400]
            if {$csr & 0x80} break
            if {$i > 1000000} {
                error [format "rlseek: failed to see RDY in RLCS %04X getting status" $csr]
            }
            if [ctrlcflag] return
        }
        if {$csr & 0x8000} {
            error [format "rlseek: error RLCS %04X getting status" $csr]
        }
        set dsr [rdword 0774406]                    ;# get drive status
        if {($dsr & 7) != 5} {
            error [format "rlseek: drive $drive RLDS %04X not ready" $dsr]
        }
        if {! ($dsr & 0x200)} break
        puts "rlseek: drive $drive volume check"
        set gsdar 013
    }

    set curtrk [expr {($dsr >> 6) & 1}]             ;# get current track (head number)

    # read header to get current cylinder number
    wrword 0774400 [expr {($drive << 8) | 8}]       ;# set CSR = 000010 | driveselect
    for {set i 0} true {incr i} {
        set csr [rdword 0774400]
        if {$csr & 0x80} break
        if {$i > 1000000} {
            error [format "rlseek: failed to see RDY in RLCS %04X reading header" $csr]
        }
        if [ctrlcflag] return
    }
    if {$csr & 0x8000} {
        error [format "rlseek: error RLCS %04X reading header" $csr]
    }
    set header [rdword 0774406]
    set curcyl [expr {($header >> 7) & 511}]
    set curtrk [expr {($header >> 6) &   1}]

    # see if seek needed
    set cyl [expr {int ($block / 80)}]
    set trk [expr {($block % 80) >= 40}]
    set delcyl [expr {$cyl - $curcyl}]
    if {($delcyl != 0) || ($trk != $curtrk)} {
        set dir 4
        if {$delcyl <= 0} {
            set dir 0
            set delcyl [expr {0 - $delcyl}]
        }
        wrword 0774404 [expr {($delcyl << 7) | ($trk << 4) | $dir | 1}]
        wrword 0774400 [expr {($drive << 8) | 6}]

        for {set i 0} true {incr i} {
            set csr [rdword 0774400]
            if {$csr & 0x80} break
            if {$i > 1000000} {
                error [format "rlseek: failed to see RDY in RLCS %04X starting seek" $csr]
            }
            if [ctrlcflag] return
        }
        if {$csr & 0x8000} {
            error [format "rlseek: error RLCS %04X starting seek" $csr]
        }
    }
}

proc rlwrite {drive block addr {wcnt 128}} {
    rlseek $drive $block
    set cyl [expr {int ($block / 80)}]
    set trk [expr {($block % 80) >= 40}]
    set sec [expr {$block % 40}]
    wrword 0774406 [expr {0200000 - $wcnt}]
    wrword 0774404 [expr {($cyl << 7) | ($trk << 6) | $sec}]
    wrword 0774402 [expr {$addr & 0177777}]
    wrword 0774400 [expr {($drive << 8) | (($addr >> 16) << 4) | 10}]
    for {set i 0} true {incr i} {
        set csr [rdword 0774400]
        if {$csr & 0x80} break
        if {$i > 1000000} {error [format "rlwrite: failed to see RDY in RLCS %04X writing block" $csr]}
        if [ctrlcflag] return
    }
    if {$csr & 0x8000} {
        error [format "rlwrite: error RLCS %04X writing block $block" $csr]
    }
}

# fill nblks starting at block with random contents
# use memory starting at addr for storage
proc fillrandblock {drive block nblks addr} {
    global savedwords

    set nwrds [expr {$nblks * 128}]

    # fill memory with random data
    for {set i 0} {$i < $nwrds} {incr i} {
        set data [myrand 65536]
        set wordnum [expr {$drive * 512 * 2 * 40 * 128 + $block * 128 + $i}]
        set savedwords($wordnum) $data
        wrword [expr {$addr + $i * 2}] $data
    }

    # write to disk
    puts [format "writing %d %05d..%05d using %06o" $drive $block [expr {$block + $nblks - 1}] $addr]
    rlwrite $drive $block $addr $nwrds
}

proc myrand {top} {
    return [expr {int (rand () * $top)}]
}

########################################

pin set bm_enablo 0xFFFFFFFF bm_enabhi 0x3FFFFFFF

set maxblock 200    ;# multiple of 40

# fill disk with random contents
for {set drive 0} {$drive < 4} {incr drive} {
    for {set block 0} {$block < $maxblock} {incr block $nblks} {
        # get number of blocks to write
        set nblkstoendoftrack [expr {40 - ($block % 40)}]
        set nblks [expr {[myrand $nblkstoendoftrack] + 1}]
        set nwrds [expr {$nblks * 128}]
        # get memory address to start at
        set addr [expr {[myrand [expr {0760000 - $nwrds * 2}]] & 0777776}]

        fillrandblock $drive $block $nblks $addr
        # stop if control-C
        if [ctrlcflag] return
    }
}

# read random blocks and verify
while {! [ctrlcflag]} {
    # get random drive number
    set drive [myrand 4]
    # get random block number and random number of blocks
    set block [myrand $maxblock]
    set nblkstoendoftrack [expr {40 - ($block % 40)}]
    set nblks [expr {[myrand $nblkstoendoftrack] + 1}]
    set nwrds [expr {$nblks * 128}]
    # get memory address to start at
    set addr [expr {[myrand [expr {0760000 - $nwrds * 2}]] & 0777776}]

    if {[myrand 16] == 0} {

        # occasionally write
        fillrandblock $drive $block $nblks $addr
        if [ctrlcflag] return
    } else {

        # read from disk
        puts [format "reading %d %05d..%05d using %06o" $drive $block [expr {$block + $nblks - 1}] $addr]
        rlread $drive $block $addr $nwrds
        # stop if control-C
        if [ctrlcflag] return
        # verify memory contents
        for {set i 0} {$i < $nwrds} {incr i} {
            set wordnum [expr {$drive * 512 * 2 * 40 * 128 + $block * 128 + $i}]
            set savdata $savedwords($wordnum)
            set memaddr [expr {$addr + $i * 2}]
            set memdata [rdword $memaddr]
            if {$savdata != $memdata} {
                error [format "data difference %06o \[%05o\] %06o sb %06o" $memaddr $i $memdata $savdata]
            }
        }
    }
}
