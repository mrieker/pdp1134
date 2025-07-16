#!/bin/bash
cd `dirname $0`
nohup ./z11xe -killit < /dev/null > ~/z11xe.log 2>&1 &
exec ./z11ctrl rsx45sysgen-decnet.tcl
