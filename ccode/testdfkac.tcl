#
#  prints END PASS within 2 sec
#  then prints every 5 sec thereafter
#
enabmem
hardreset
loadbin dfkac.bin
puts "fpgamode=[pin fpgamode] muxdelay=[pin muxdelay]"
flickstart 0200
exec -ignorestderr ./z11dl -killit < /dev/tty > /dev/tty
