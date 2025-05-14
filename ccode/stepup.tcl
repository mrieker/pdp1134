
# Step 11/34 through power-up

# Plug UC1134 board into microstep connector
# Only other board is console, no memory
# Boot UC1134 board as hostname zturn3
# Make sure PDP turned off
# Run this script on main 1134 board
#  $ ./z11ctrl
#  > source stepup.tcl
#  > stepup
# Turn PDP on
# Press control-C after a couple seconds

proc stepup {} {
    pin set fpgamode 0 fpgamode 2
    pin set bm_enablo 0xFFFFFFFF

    puts "stepup: doing ssh to zturn3"
    exec rm -f /tmp/ucstdin /tmp/ucstdout
    exec mkfifo /tmp/ucstdin
    exec mkfifo /tmp/ucstdout
    spawn -stdin /tmp/ucstdin -stdout /tmp/ucstdout ssh zturn3 nfs/uc1134/ccode/ucpin

    puts "stepup: opening ssh pipes"
    set ucstdin [open /tmp/ucstdin w]
    set ucstdout [open /tmp/ucstdout]
    puts "stepup: writing initial commands"
    puts $ucstdin "pin set manlen 10 mancnt 10 manmode 1"
    puts $ucstdin "puts DONE"
    flush $ucstdin
    puts "stepup: waiting for echo"
    while {! [ctrlcflag] && ([gets $ucstdout x] >= 0)} {
        puts "stepup: echo <$x>"
        if {$x == "DONE"} break
    }

    set logname "stepup.[clock format [clock seconds] -format %m%d%H%M%S].log"
    set logfile [open $logname w]
    puts "stepup: logging to $logname"
    set oldupc -1
    while {! [ctrlcflag]} {
        set oldilaindex [pin get ilaindex]
        pin set ilaafter 4000 ilaarmed 1
        puts $ucstdin "pin set mancnt 0 get upc"
        flush $ucstdin
        set newupc [gets $ucstdout]
        if {[string range $newupc 0 5] == "ucpin>"} {
            set newupc [gets $ucstdout]
        }
        if {$oldupc != $newupc} {
            set oldupc $newupc
            puts [format "upc %03o" $newupc]
            puts $logfile [format "upc %03o" $newupc]
            set newilaindex [pin get ilaindex set ilaafter 0 ilaarmed 0]
            for {set i $oldilaindex} {$i != $newilaindex} {set i [expr {($i + 1) & 4095}]} {
                pin set ilaindex $i
                set ila0 [pin get ilartime]
                set ila1 [pin get ilardatahi]
                set ila2 [pin get ilardatalo]
                puts [format "%04o  %08X %08X %08X" $i $ila0 $ila1 $ila2]
                puts $logfile [format "%04o  %08X %08X %08X" $i $ila0 $ila1 $ila2]
            }
        }
    }
    close $logfile
}
