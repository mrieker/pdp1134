
MACH := $(shell uname -m)

verisim.$(MACH).a: verisim.$(MACH).o verilated.$(MACH).o obj_dir/VZynq__ALL.a
	rm -f tmp.a
	cp obj_dir/VZynq__ALL.a tmp.a
	ar r tmp.a verisim.$(MACH).o verilated.$(MACH).o
	mv tmp.a verisim.$(MACH).a

verisim.$(MACH).o: verisim.cc verisim.h obj_dir/VZynq.h
	g++ -g -c -o verisim.$(MACH).o -I/usr/share/verilator/include/ verisim.cc

verilated.$(MACH).o: /usr/share/verilator/include/verilated.cpp
	g++ -g -c -o verilated.$(MACH).o -I/usr/share/verilator/include/ /usr/share/verilator/include/verilated.cpp

obj_dir/VZynq__ALL.a:
	make -C obj_dir -f VZynq.mk

obj_dir/VZynq.cpp, obj_dir/VZynq.h: ../zynq/bigmem.v ../zynq/dl11.v ../zynq/intctl.v ../zynq/pc11.v ../zynq/rl11.v ../zynq/sim1134.v ../zynq/swlight.v ../zynq/zynq.v
	verilator --cc --top-module Zynq ../zynq/bigmem.v ../zynq/dl11.v ../zynq/intctl.v ../zynq/pc11.v ../zynq/rl11.v ../zynq/sim1134.v ../zynq/swlight.v ../zynq/zynq.v

