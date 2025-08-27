pin set fpgamode 1
pin set bm_enablo 0xffffffff ky_enable 1 dl_enable 1
pin set ky_switches 0100000
exec ./z11dl -killit -nokb < /dev/null > /dev/tty &
##loadbin ../maindec/20040310/maindec-11-dfkaa-b1-pb.bin
##loadbin ../maindec/20040310/maindec-11-dfkab-c-pb.bin
loadbin ../maindec/20040310/maindec-11-dfkac-a-pb.bin
flickstart 0200

