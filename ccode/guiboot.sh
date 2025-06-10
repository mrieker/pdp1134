#!/bin/bash
#
#  Script spawned by GUI BOOT button
#
#   $1 = 18-bit switch register value (in decimal)
#
export Z11DIR=`dirname $0`
exec $Z11DIR/z11ctrl $Z11DIR/guiboot.tcl "$@"
