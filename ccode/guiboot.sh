#!/bin/bash
#
#  Script spawned by GUI BOOT button
#
#   $1 = 18-bit switch register value (in decimal)
#
cd `dirname $0`
exec ./z11ctrl rsx.tcl
