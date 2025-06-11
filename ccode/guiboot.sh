#!/bin/bash
#
#  Script spawned by GUI BOOT button
#
#   $1 = 18-bit switch register value (in decimal)
#
cd `dirname $0`
#
#  rlboot.tcl - boot RL disk
#
exec ./z11ctrl rlboot.tcl "$@"
