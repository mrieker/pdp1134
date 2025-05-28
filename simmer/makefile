
MACH := $(shell uname -m)

OBJS := \
	axidev.$(MACH).o \
	bigmem.$(MACH).o \
	cpu1134.$(MACH).o \
	dl11.$(MACH).o \
	kl11.$(MACH).o \
	rl11.$(MACH).o \
	simmer.$(MACH).o \
	stepper.$(MACH).o \
	swlight.$(MACH).o \
	unidev.$(MACH).o

default: \
	simmer.$(MACH)

simmer.$(MACH): $(OBJS)
	g++ -g -o $@ $^ -lpthread

%.$(MACH).o: %.cc *.h
	g++ -Wall -Werror -g -c -o $@ $<

