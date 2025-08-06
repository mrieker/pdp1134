
# install basic-plus-2 on rsts/e v10.1 system from tapes

#  assumes system is logged into freshly created privileged account
#  and waiting at the "$" prompt (z11dl does not need to be running)

# input:
#  disks/rststapes/rsts101/rsts101/bp2_v2_7.tap
#    from https://groups.google.com/g/pidp-11/c/kStWvbpQuUA/m/8BYBQrCgAgAJ
#      about 1/2 way down - link RSTS/E 10.1

# output:
#  disks/rsts101/dbsys.rp04

# note:
#  disks should be softlink to magnetic disk so it doesn't
#  pound the daylights out of the sdcard

dllock -killit      ;# kill z11dl if it is running

tmload 0 -readonly disks/rststapes/rsts101/bp2_v2_7.tap

sendttychar "\r"    ;# get another $ prompt
replytoprompt "\$ " "@\[0,1\]INSTAL BP2"

replytoprompt "Patch account? <PATCH\$:> " ""
replytoprompt "Is this list OK? <yes> " ""
replytoprompt "Target disk? <_SY:> " ""
replytoprompt "BP2 Library device? <_MT0:>" "" 
replytoprompt "Do you wish to install the prebuilt kit? <YES> : " "NO"
replytoprompt "Do you want the default installation <YES>" ""
replytoprompt "What name do you want to use to invoke BP2 <BP2>" ""
replytoprompt "Do you wish to change any of your answers <NO>" ""
waitforstring "is a log file of this session"
waitforcrlf

puts ""
puts "= = = = = = = = = = = = = = = ="
puts "ALL DONE"
puts ""
puts "  do ./z11dl to access tty"
puts "  use BASIC command to start it"
puts ""
