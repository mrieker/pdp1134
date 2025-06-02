
MACH := $(shell uname -m)

OBJS := \
	axidev.$(MACH).o \
	bigmem.$(MACH).o \
	cpu1134.$(MACH).o \
	dl11.$(MACH).o \
	kl11.$(MACH).o \
	ky11.$(MACH).o \
	rl11.$(MACH).o \
	simmer.$(MACH).o \
	stepper.$(MACH).o \
	unidev.$(MACH).o \
	../ccode/disassem.$(MACH).o \
	../ccode/strprintf.$(MACH).o \

default: \
	simmer.$(MACH) \
	simrpage.$(MACH).o

simmer.$(MACH): $(OBJS)
	g++ -g -o $@ $^ -lpthread

%.$(MACH).o: %.cc *.h
	g++ -Wall -Werror -fPIC -g -c -o $@ $<

