#
#  prints END OF DFKAB within 5 sec
#  then every 25 to 30 sec thereafter
#
enabmem
hardreset
loadbin dfkab.bin
puts "fpgamode=[pin fpgamode] muxdelay=[pin muxdelay]"
flickstart 0200
exec -ignorestderr ./z11dl -killit < /dev/tty > /dev/tty
