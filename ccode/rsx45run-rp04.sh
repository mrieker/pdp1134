#!/bin/bash
#
#  Boot RSX 4.5 built via rsx45sysgen-rp04.sh
#
./z11xe -daemon
./z11rh &
./z11rl &
./z11tm &
exec ./z11ctrl rsx45run-rp04.tcl
