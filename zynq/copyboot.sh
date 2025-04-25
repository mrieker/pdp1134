#!/bin/bash
#
#  Copy BOOT.BIN.gz to ZTurn boot block
#  Run on ZTurn
#
set -e -v
gunzip -c BOOT.BIN.gz | sudo dd of=/boot/BOOTX.BIN bs=4096
sudo mv /boot/BOOTX.BIN /boot/BOOT.BIN
sudo reboot
