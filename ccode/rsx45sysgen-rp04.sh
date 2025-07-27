#!/bin/bash
cd `dirname $0`
./z11xe -daemon -killit
exec ./z11ctrl rsx45sysgen-rp04.tcl
