
# Called by guiboot.sh as a result of the GUI's BOOT button
# Assumes RL02 drive 0 (real or fake) is loaded with boot disk

set fpgamode [pin get fpgamode]
if {($fpgamode != 1) && ($fpgamode != 2)} {
    puts "guiboot.tcl: fpga OFF - can't boot"
    return
}

# clobber processor if it was babbling
hardreset

# make sure we have some memory
enabmem

# make sure we have basic devices
if {[rdwordtimo 0774400] < 0} {
    pin set rl_enable 1
}
if {[rdwordtimo 0777546] < 0} {
    pin set kw_enable 1 kw_fiftyhz 1
}
if {[rdwordtimo 0777560] < 0} {
    pin set dl_enable 1
}
if {[rdwordtimo 0777570] < 0} {
    pin set ky_enable 1
}

# toggle in RL02 drive 0 boot
set z11dir [getenv Z11DIR ""]
loadlst $z11dir/rlboot.lst

# start it going
flickstart 010000

# if synth tty, say how to access it
set ttmsg ""
if {[pin get dl_enable]} {
    set ttmsg " use $z11dir/z11dl for tty access"
}
puts "guiboot.tcl: RL02 drive 0 booting...$ttmsg"
