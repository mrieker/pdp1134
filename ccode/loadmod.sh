#!/bin/bash
cd `dirname $0`
if grep -q 'Xilinx Zynq' /proc/cpuinfo
then
    if ! lsmod | grep -q ^zynqpdp11
    then
        if [ ! -f km-zynqpdp11/zynqpdp11.`uname -r`.ko ]
        then
            cd km-zynqpdp11
            rm -f zynqpdp11.ko zynqpdp11.mod* zynqpdp11.o modules.order Module.symvers .zynqpdp11* .modules* .Module*
            make
            mv zynqpdp11.ko zynqpdp11.`uname -r`.ko
            cd ..
        fi
        sudo insmod km-zynqpdp11/zynqpdp11.`uname -r`.ko
    fi
fi
