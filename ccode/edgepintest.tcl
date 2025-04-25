
# test pins coming out of zynq/zturn board
#  ./z8lpin pintest.tcl

proc splitcsvline {csvline} {
    set len [string length $csvline]
    set csvlist {}
    set quoted 0
    set strsofar ""
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $csvline $i]
        if {! $quoted && ($ch == ",")} {
            lappend csvlist $strsofar
            set strsofar ""
            continue
        }
        if {$ch == "\""} {
            set quoted [expr {! $quoted}]
            continue
        }
        if {$ch == "\\"} {
            incr i
            set ch [string index $csvline $i]
        }
        set strsofar "$strsofar$ch"
    }
    lappend csvlist $strsofar
    return $csvlist
}

# test inputonly pins
#  need unresistored led pullup to +5V
proc testins {} {
    global edgepindict sortedkeys

    for {set side 1} {$side <= 2} {incr side} {
        foreach edgepin $sortedkeys {
            if {[string last $side $edgepin] == 2} {
                set val [dict get $edgepindict $edgepin]
                set pintype [lindex $val 0]
                if {$pintype == "inputonly"} {
                    testpin $edgepin
                    after 1000
                    if {[ctrlcflag 0]} return
                }
            }
        }
    }
}

# test bidirectional/outputonly pins
#  need resistored led pullup to +5V
proc testouts {} {
    global edgepindict sortedkeys

    for {set side 1} {$side <= 2} {incr side} {
        foreach edgepin $sortedkeys {
            if {[string last $side $edgepin] == 2} {
                set val [dict get $edgepindict $edgepin]
                set pintype [lindex $val 0]
                if {$pintype != "inputonly"} {
                    testpin $edgepin
                    after 1000
                    if {[ctrlcflag 0]} return
                }
            }
        }
    }
}

# test one pin
proc testpin {edgepin} {
    global edgepindict

    set edgepin [string toupper $edgepin]
    set val [dict get $edgepindict $edgepin]
    puts ""
    puts " $edgepin $val"
    set pintype [lindex $val 0]
    set on 0
    while {! [ctrlcflag 0]} {
        switch $pintype {
            inputonly {
                set ison [pin get [lindex $val 1]]
                puts " $edgepin -> $ison"
            }
            outputonly {
                pin set man_[lindex $val 1] $on
                puts " $edgepin <- $on"
            }
            dedinput {
                pin set man_[lindex $val 3] $on
                set ison [pin get [lindex $val 1]]
                puts " $edgepin <- $on -> $ison"
            }
            dedoutput {
                pin set man_[lindex $val 1] $on
                set ison [pin get [lindex $val 3]]
                puts " $edgepin <- $on -> $ison"
            }
            muxfeedback {
                set zynqsig [lindex $val 1]
                pin set man_$zynqsig $on
                set rsel [string index [lindex $val 2] 0]
                set muxx [string index [lindex $val 2] 1]
                set muxx [string tolower $muxx]
                if {$rsel == 1} {pin set man_rsel1_h 1 man_rsel2_h 0 man_rsel3_h 0}
                if {$rsel == 2} {pin set man_rsel1_h 0 man_rsel2_h 1 man_rsel3_h 0}
                if {$rsel == 3} {pin set man_rsel1_h 0 man_rsel2_h 0 man_rsel3_h 1}
                set muxedfeedback [pin get mux$muxx]
                set latchedfeedback [pin get [string map {_out_ _in_} $zynqsig]]
                pin set man_rsel1_h 0 man_rsel2_h 0 man_rsel3_h 0
                puts " $edgepin <- $on -> $muxedfeedback -> $latchedfeedback"
            }
        }
        set on [expr {! $on}]
        after 1000
    }
}

# SCRIPT STARTS HERE

# read csv file for pin definitions
set csvfile [open "../bussigs.csv" "r"]
set edgeindict [dict create]
while true {
    set csvline [gets $csvfile]
    if {$csvline == ""} break
    set columns [splitcsvline $csvline]

    set signame [lindex $columns 0]
    if {$signame == ""} continue
    if {[string index $signame 0] == " "} continue

    set edgepin [lindex $columns 1]
    set muxpin  [lindex $columns 3]
    set zynqsig [lindex $columns 7]

    # feedback via mux pin
    #  BUS_A02_L,CF1,"Q13A,B",2C,J12-44,B35-LN10,J19,a_out_h[02],
    if {$muxpin != ""} {
        dict append edgepindict $edgepin "muxfeedback $zynqsig $muxpin "
        continue
    }

    # feedback via dedicated pin
    #  BUS_BBSY_L(O),DD1,Q22B,,J12-27,B35-LP03,E17,bbsy_out_h,
    #  BUS_BBSY_L(I),DD1,Q22A,,J12-29,B35-LN03,D18,bbsy_in_h,
    if {[string last {(I)} $signame] >= 0} {
        dict append edgepindict $edgepin "dedinput $zynqsig "
        continue
    }
    if {[string last {(O)} $signame] >= 0} {
        dict append edgepindict $edgepin "dedoutput $zynqsig "
        continue
    }

    # input only
    #  BUS_AC_LO_L,AV1,Q08A,,J11-70,LCD-VSYNC,V16,ac_lo_in_h,
    if {[string last "_in_" $zynqsig] >= 0} {
        dict append edgepindict $edgepin "inputonly $zynqsig "
        continue
    }

    # output only
    #  BUS_BG4_OUT_H,BT2,Q53B,,J11-69,LCD-HSYNC,W16,bg_out_l[4],
    if {[string last "_out_" $zynqsig] >= 0} {
        dict append edgepindict $edgepin "outputonly $zynqsig "
        continue
    }
}
close $csvfile

# print out list of edgepins
set sortedkeys [lsort [dict keys $edgepindict]]
foreach edgepin $sortedkeys {
    puts " $edgepin [dict get $edgepindict $edgepin]"
}

# set manual pin control mode
pin set fpgamode 3

puts ""
puts " testins           - test inputonly pins"
puts "                     need unresistored led pullup to +5V"
puts " testouts          - test bidirectional/outputonly pins"
puts "                     need resistored led pullup to +5V"
puts " testpin <pinname> - test single pin"
puts ""
puts " control-C to go on to next pin, double-control-C to abort"
puts ""
