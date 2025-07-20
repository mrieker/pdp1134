# booting functions

# probe devices and memory
# fill in anything missing with zynq devices and memory
# if sim mode, turns everything on cuz it doesn't find any real devs or mem
proc probedevsandmem {} {

    enablerealdma                   ;# make sure we can read & write unibus
    pin set dz_enable 0             ;# disable zynq DZ-11
    if {[rdwordtimo 0760100] < 0} { ;# check for real DZ-11
        pin set dz_addres 0760100   ;# if not, enable zynq DZ-11
        pin set dz_intvec 0300
        pin set dz_enable 1
    }
    pin set dl_enable 0             ;# disable zynq tty
    if {[rdwordtimo 0777560] < 0} { ;# see if real tty exists
        pin set dl_enable 1         ;# if not, enable zynq tty
    }
    pin set kw_enable 0             ;# same with line clock
    if {[rdwordtimo 0777546] < 0} {
        pin set kw_enable 1
    }
    pin set pc_enable 0             ;# same with pc controller
    if {[rdwordtimo 0777550] < 0} {
        pin set pc_enable 1
    }
    pin set rh_enable 0             ;# same with rh controller
    if {[rdwordtimo 0776700] < 0} {
        pin set rh_enable 1
    }
    pin set rl_enable 0             ;# same with rl controller
    if {[rdwordtimo 0774400] < 0} {
        pin set rl_enable 1
    }
    pin set tm_enable 0             ;# same with tm controller
    if {[rdwordtimo 0772520] < 0} {
        pin set tm_enable 1
    }
    pin set xe_enable 0             ;# same with xe controller
    if {[rdwordtimo 0774510] < 0} {
        pin set xe_enable 1
    }
    pin set bm_enablo 0 bm_enabhi 0 ;# same with main mem
    set mem 0                       ;# except probe page-by-page
    for {set i 0} {$i < 62} {incr i} {
        set a [expr {$i << 12}]
        if {[rdwordtimo $a] < 0} {
            set mem [expr {$mem | (1 << $i)}]
        }
    }
    pin set bm_enablo [expr {$mem & 0xFFFFFFFF}] bm_enabhi [expr {$mem >> 32}]
}

# make sure rdword,wrword can access unibus
# zynq is in real mode and processor has been reset
# will also work in sim mode, it will think there is no real KY-11
proc enablerealdma {} {

    # make sure processor is reset and halted
    hardreset

    # check for presence of real KY-11 by looking for BBSY stuck on
    set n 0
    for {set i 0} {$i < 5} {incr i} {
        after 1
        incr n [pin dev_bbsy_h]
    }
    switch $n {
        0 {
            # no BBSY seen, real KY-11 is not present, rdword,wrword should just work
            pin set ky_enable 1 ;# enable zynq ky11.v 777570 registers
        }
        5 {
            # BBSY stuck on, presumably by real KY-11, persuade it to release BBSY
            releasebbsy
        }
        default {
            puts "boots.tcl: unable to determine presence of real KY-11 console board"
            after 2000
            puts "boots.tcl: (BBSY line flickering), cannot boot"
            after 2000
            puts "boots.tcl: make sure processor is halted then try again"
            after 2000
            exit
        }
    }
}

# get processor running a wait loop so real KY-11 will release BBSY
proc releasebbsy {} {

    set bootrombase 0761000

    # put an infinite loop of WAIT/BR.-2 in zynq boot mem
    # bmrdword,bmwrword access zynq mem via backdoor not using unibus
    bmwrword [expr {$bootrombase+0000}] 0000000 ;# flagword
    bmwrword [expr {$bootrombase+0024}] [expr {$bootrombase-0600000+0100}] ;# power-up vector
    bmwrword [expr {$bootrombase+0026}] 0000340
    bmwrword [expr {$bootrombase+0100}] 0005267 ;# inc flagword
    bmwrword [expr {$bootrombase+0102}] 0176774
    bmwrword [expr {$bootrombase+0104}] 0000001 ;# wait
    bmwrword [expr {$bootrombase+0106}] 0000776 ;# br .-2

    startbootmem $bootrombase   ;# start it up

    # look for flagword set indicating processor started running our program
    for {set i 0} {[bmrdword $bootrombase] != 1} {incr i} {
        if {$i > 1000} {
            puts "boots.tcl: failed to start processor"
            after 2000
            puts "boots.tcl: possibly M9312 S1-2 is ON, it must be OFF"
            after 2000
            puts "boots.tcl: check then retry boot"
            after 2000
            exit
        }
        after 1
    }

    # processor is supposedly running WAIT/BR.-2 loop so we can do dma
    # regular rdword should be able to read the boot mem now
    if {[rdword [expr {$bootrombase+0102}]] != 0176774} {
        error "releasebbsy: verify boot mem read failed"
    }

    # disable zynq ky11.v 777570 registers cuz real KY-11 has that address
    pin set ky_enable 0
}

# start program loaded into zynq boot memory bigmem.v at bootrombase
# assumes program loaded into boot memory has power-up vector set up
#  input:
#   bootrombase = where boot program was loaded in bigmem.v memory
#                 0760000, 0761000, 0762000, ,..., 0777000
proc startbootmem {bootrombase} {

    if {($bootrombase < 0760000) || ($bootrombase > 0777000) || ($bootrombase & 0777)} {
        error [format "startbootmem: bad bootrombase %06o" $bootrombase]
    }
    set brjama [expr {($bootrombase-0760000)>>9}]
    set brenab [expr {1<<$brjama}]

    pin set bm_brenab $brenab   ;# plug bootrombase boot mem into unibus
    pin set bm_brjama $brjama   ;# set up bootrombase to be jammed on unibus address lines
    pin set man_ac_lo_out_h 1   ;# powerfail the processor
    after 5
    pin set man_dc_lo_out_h 1
    pin set bm_brjame 1         ;# jam bootrombase onto address lines
    pin set ky_haltreq 0        ;# make sure we aren't requesting it to halt when it starts back up
    after 200
    pin set man_dc_lo_out_h 0   ;# power-up processor
    pin set man_ac_lo_out_h 0   ;# it should read power-up vector and run
                                ;# bm_brjame should self-clear when processor finishes reading power-up vector
}

# boot PR bin file
proc prboot {} {
    global Z11HOME
    if {[pin pc_enable]} {      ;# if using fpga paper tape reader
        wrtty "on another screen do:\n"
        wrtty "  $Z11HOME/z11pc reader <filename>\n"
    }
    wrtty "press return when ready or esc to abandon boot: "
    while {! [ctrlcflag]} {
        set by [rdtty]
        if {$by == 015} {
            wrtty "\nreading in paper tape\n"
            loadlst $Z11HOME/absldr.lst bmwr
            startbootmem 0761000
            return
        }
        if {$by == 033} {
            wrtty "\nabandoning boot\n"
            return
        }
    }
}

# boot RL disk 0
proc rlboot {} {
    global Z11HOME
    loadlst $Z11HOME/rlboot.lst bmwr
    startbootmem 0761000
}

# boot TM tape 0
proc tmboot {} {
    global Z11HOME
    loadlst $Z11HOME/tm11-boot.lst bmwr
    bmwrword 0773024 0173004
    startbootmem 0773000
}
