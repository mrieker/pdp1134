#
#  prints END OF DFKAA-B within 3 sec
#  then prints every 10 sec thereafter
#
enabmem
hardreset
loadbin dfkaa.bin
puts "fpgamode=[pin fpgamode] muxdelay=[pin muxdelay]"
flickstart 0200
exec -ignorestderr ./z11dl -killit < /dev/tty > /dev/tty
