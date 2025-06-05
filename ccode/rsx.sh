#!/bin/bash
export z11rl_debug=1
./z11rl -killit \
  -loadrw 0 $1/rsx11m4.1_sys_34.rl02 \
  -loadrw 1 $1/rsx11m4.1_user.rl02   \
  -loadrw 2 $1/rsx11m4.1_hlpdcl.rl02 \
  -loadrw 3 $1/rsx11m4.1_excprv.rl02 > ~/rsx-rl02.log 2>&1
