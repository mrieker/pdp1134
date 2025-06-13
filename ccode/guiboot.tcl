puts "guiboot.tcl: started"

# start program loaded into boot memory at 761000
# assumes program loaded into boot memory has power-up vector set up
proc startbootmem {} {
    pin set bm_brenab 2         ;# plug 761xxx boot mem into unibus
    pin set bm_brjama 1         ;# set up 761000 to be jammed on unibus address lines
    pin set man_ac_lo_out_h 1   ;# powerfail the processor
    after 5
    pin set man_dc_lo_out_h 1
    pin set bm_brjame 1         ;# jam 761000 onto address lines
    pin set ky_haltreq 0        ;# make sure we aren't requesting it to halt when it starts back up
    after 200
    pin set man_dc_lo_out_h 0   ;# power-up processor
    pin set man_ac_lo_out_h 0   ;# it should read power-up vector and run
                                ;# bm_brjame should self-clear when processor reads power-up vector
}

# zynq is in real mode and processor has been reset
# make sure rdword,wrword can access unibus
proc enablerealdma {} {

    # check for presence of KY-11 by looking for BBSY stuck on
    set n 0
    for {set i 0} {$i < 5} {incr i} {
        after 1
        incr n [pin dev_bbsy_h]
    }
    switch $n {
        0 {
            pin set ky_enable 1 ;# enable zynq 777570 registers
            return  ;# no BBSY seen, KY-11 is not present, dma will just work
        }
        5 { }
        default {
            puts "guiboot.tcl: unable to determine presence of KY-11 console board"
            after 2000
            puts "guiboot.tcl: (BBSY line flickering), cannot boot"
            after 2000
            exit
        }
    }

    # we have to get processor running so KY-11 will release BBSY
    # put an infinite loop of WAIT/BR.-2 in zynq boot mem
    # bmrdword,bmwrword access zynq mem via backdoor not using unibus
    bmwrword 0761000 0000000    ;# flagword
    bmwrword 0761024 0161100    ;# power-up vector
    bmwrword 0761026 0000340
    bmwrword 0761100 0005237    ;# inc flagword
    bmwrword 0761102 0161000
    bmwrword 0761104 0000001    ;# wait
    bmwrword 0761106 0000776    ;# br .-2

    startbootmem                ;# start it up

    # look for flagword set indicating processor started running our program
    for {set i 0} {[bmrdword 0761000] != 1} {incr i} {
        if {$i > 1000} {
            puts "guiboot.tcl: failed to start processor"
            after 2000
            puts "guiboot.tcl: possibly M9312 S1-2 is ON, it must be OFF"
            after 2000
            puts "guiboot.tcl: check then retry boot"
            after 2000
            exit
        }
        after 1
    }

    # processor is supposedly running WAIT/BR.-2 loop so we can do dma
    # regular rdword should be able to read the boot mem now
    if {[rdword 0761024] != 0161100} {
        error "enablerealdma: verify boot mem read failed"
    }

    # disable zynq 777570 registers
    pin ky_enable 0
}

# write string to tty
proc wrtty {str} {
    set len [string length $str]
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $str $i]
        if {$ch == "\n"} {wrtty "\r"}
        while {! [ctrlcflag] && ! ([rdword 0777564] & 0200)} {after 1}
        scan $ch "%c" by
        wrbyte 0777566 $by
    }
}

# read single char from tty
proc rdtty {} {
    while {! [ctrlcflag] && ! ([rdword 0777560] & 0200)} {after 1}
    return [rdbyte 0777562]
}

# boot RL disk 0
proc rlboot {} {
    wrtty "\nloading rlboot file\n"
    loadlst rlboot.lst bmwr
    wrtty "starting rlboot\n"
    startbootmem
}

##  SCRIPT STARTS HERE  ##

hardreset

#set fpgamode [pin fpgamode]
#switch $fpgamode {
#   1 {
#       puts "guiboot.tcl: SIM mode - enabling all devices and memory"
#       pin set dl_enable 1 kw_enable 1 ky_enable 1 rl_enable 1
#       pin set bm_enablo 0xFFFFFFFF bm_enabhi 0x3FFFFFFF
#   }
#   2 {
        puts "guiboot.tcl: REAL mode - probing devices and memory"
        enablerealdma                   ;# make sure we can read & write unibus
        pin set dl_enable 0             ;# disable zynq tty
        if {[rdwordtimo 0777560] < 0} { ;# see if real tty exists
            pin set dl_enable 1         ;# if not, enable zynq tty
        }
        pin set kw_enable 0             ;# same with line clock
        if {[rdwordtimo 0777546] < 0} {
            pin set kw_enable 1
        }
        pin set rl_enable 0             ;# same with rl controller
        if {[rdwordtimo 0774400] < 0} {
            pin set rl_enable 1
        }
        pin set bm_enablo 0 bm_enabhi 0 ;# same with main mem
        set mem 0
        for {set i 0} {$i < 62} {incr i} {
            set a [expr {$i << 12}]
            if {[rdwordtimo $a] < 0} {
                set mem [expr {$mem | (1 << $i)}]
            }
        }
        pin set bm_enablo [expr {$mem & 0xFFFFFFFF}] bm_enabhi [expr {$mem >> 32}]
#   }
#   default {
#       puts "guiboot.tcl: zynq turned OFF - select REAL or SIM then press BOOT again"
#       exit
#   }
#}

if {[pin dl_enable]} {
    puts "guiboot.tcl: use z11dl -cps 960 to access zynq tty"
} else {
    puts "guiboot.tcl: using hardware tty controller"
}

wrtty "\n"
wrtty "Select\n"
wrtty "  0) cancel boot\n"
wrtty "  1) boot from RL-11 drive 0\n"
wrtty "> "

while {! [ctrlcflag]} {
    set by [rdtty]
    if {$by == 060} {
        wrtty "\nabandoning boot\n"
        return
    }
    if {$by == 061} {
        rlboot
        return
    }
}


