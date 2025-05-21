
MACH   := $(shell uname -m)

VFILES := \
	myboard.v \
	../zynq/bigmem.v \
	../zynq/dl11.v \
	../zynq/intctl.v \
	../zynq/pc11.v \
	../zynq/rl11.v \
	../zynq/sim1134.v \
	../zynq/swlight.v \
	../zynq/zynq.v


default: verimain.$(MACH) verisim.$(MACH).a


verisim.$(MACH).a: verisim.$(MACH).o
	rm -f verisim.$(MACH).a
	ar rc verisim.$(MACH).a verisim.$(MACH).o

verisim.$(MACH).o: verisim.cc verisim.h obj_dir/VMyBoard.h
	g++ -g -c -o verisim.$(MACH).o verisim.cc


verimain.$(MACH): verimain.$(MACH).o verilated.$(MACH).o obj_dir/VMyBoard__ALL.a
	g++ -g -o verimain.$(MACH) verimain.$(MACH).o verilated.$(MACH).o obj_dir/VMyBoard__ALL.a

verimain.$(MACH).o: verimain.cc verisim.h obj_dir/VMyBoard.h
	g++ -g -c -o verimain.$(MACH).o -I/usr/share/verilator/include/ verimain.cc

verilated.$(MACH).o: /usr/share/verilator/include/verilated.cpp
	g++ -g -c -o verilated.$(MACH).o -I/usr/share/verilator/include/ /usr/share/verilator/include/verilated.cpp

obj_dir/VMyBoard__ALL.a: obj_dir/VMyBoard.mk
	make -C obj_dir -f VMyBoard.mk

obj_dir/VMyBoard.cpp obj_dir/VMyBoard.h obj_dir/VMyBoard.mk: $(VFILES)
	rm -rf obj_dir
	verilator --cc --top-module MyBoard $(VFILES)

