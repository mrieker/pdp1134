#!/bin/bash
#
#  Script spawned by GUI BOOT button
#
#   $1 = 18-bit switch register value (in decimal)
#   $2 = 18-bit address (decimal) last loaded with LD.AD
#         or accessed with EXAM or DEP
#   $3 = "gui" : booted via GUI BOOT button
#       "pdpi" : booted via pidp-11 panel (z11pidp) data knob button
#
cd `dirname $0`
exec ./z11ctrl guiboot.tcl "$@"
