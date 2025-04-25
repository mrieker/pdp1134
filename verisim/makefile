
test: test.o obj_dir/Vpdp1134__ALL.a
	g++ -g -o test test.o obj_dir/Vpdp1134__ALL.a

test.o: test.cc obj_dir/Vpdp1134.h
	g++ -g -c -o test.o -I/usr/share/verilator/include/ test.cc

obj_dir/Vpdp1134__ALL.a: obj_dir/Vpdp1134.cpp obj_dir/verilated.o
	rm -f obj_dir/Vpdp1134__ALL.a
	make -C obj_dir -f Vpdp1134.mk
	ar r obj_dir/Vpdp1134__ALL.a obj_dir/verilated.o

obj_dir/Vpdp1134.cpp, obj_dir/Vpdp1134.h: pdp1134.v
	verilator --cc pdp1134.v

obj_dir/verilated.o: /usr/share/verilator/include/verilated.cpp
	g++ -g -c -o obj_dir/verilated.o -I/usr/share/verilator/include/ /usr/share/verilator/include/verilated.cpp

