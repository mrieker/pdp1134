#!/bin/bash
#
#  Copy BOOT.BIN.gz to ZTurn boot block
#  Run on ZTurn
#
set -e -v
cd `dirname $0`
if ! grep 'Xilinx Zynq' /proc/cpuinfo
then
    echo "doesn't look like a zynq"
    exit 1
fi
gunzip -c BOOT.BIN.gz | sudo dd of=/boot/BOOTX.BIN bs=4096
sudo mv /boot/BOOTX.BIN /boot/BOOT.BIN
sudo reboot
