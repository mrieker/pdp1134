
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
    set oldupc "xxx"
    set oldilaindex [pin get ilaindex]
    while {! [ctrlcflag]} {
        pin set ilaafter 4000 ilaarmed 1
        puts $ucstdin "pin set mancnt 0 get upc"
        flush $ucstdin
        set newupc [string trim [gets $ucstdout]]
        if {[string range $newupc 0 5] == "ucpin>"} {
            set newupc [string trim [gets $ucstdout]]
        }
        set newupc [format "%03o" $newupc]
        if {$oldupc != $newupc} {
            # dump unibus trace
            set newilaindex [pin get ilaindex set ilaafter 0 ilaarmed 0]
            set lasttime -1
            for {set i $oldilaindex} {$i != $newilaindex} {set i [expr {($i + 1) & 4095}]} {
                pin set ilaindex $i
                set ila0 [pin get ilartime]
                set ila1 [pin get ilardatahi]
                set ila2 [pin get ilardatalo]
                if {$lasttime < 0} {
                    set dt "         "
                } else {
                    set dt [format "=%5d0nS" [expr {$ila0 - $lasttime}]]
                }
                logit $logfile [format "  %04o  %08X%s %08X %08X" $i $ila0 $dt $ila1 $ila2]
                set lasttime $ila0
            }
            set oldilaindex $newilaindex
            # dump new upc - seems to be upc that is about to be executed
            # ...the above trace was for previous step
            set upcomm [getcom $newupc]
            logit $logfile ""
            logit $logfile "upc $newupc  $upcomm"
            set oldupc $newupc
        }
    }
    close $logfile
}

proc logit {logfile line} {
    puts $line
    puts $logfile $line
}

proc getcom {upc} {
    global comments

    if {! [info exists comments]} {
        set comments [dict create]
        set comfile [open "../../uc1134/ccode/ucode-comments.txt"]
        set page 0
        while {[gets $comfile line] >= 0} {
            set line [string trimright $line]
            if {$line == ""} continue
            if {[string index $line 0] <= " "} continue
            if {[string index $line 0] == "p"} {
                set page [string range $line 1 end]
                continue
            }
            set cupc [string range $line 0 2]
            set comm [string trim [string range $line 3 end]]
            if {[dict exists $comments $cupc]} {
                error "duplicate comment $cupc"
            }
            if {[string first ":" $comm] < 0} {set comm "  $comm"}
            dict set comments $cupc [format "p%02d %s" $page $comm]
        }
        close $comfile
    }

    if {[dict exists $comments $upc]} {return [dict get $comments $upc]}
    return ""
}
