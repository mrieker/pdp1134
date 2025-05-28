
MACH := $(shell uname -m)

default: \
	simmer.$(MACH)

simmer.$(MACH): axidev.$(MACH).o bigmem.$(MACH).o cpu1134.$(MACH).o dl11.$(MACH).o kl11.$(MACH).o simmer.$(MACH).o swlight.$(MACH).o unidev.$(MACH).o
	g++ -g -o $@ $^ -lpthread

%.$(MACH).o: %.cc *.h
	g++ -Wall -Werror -g -c -o $@ $<

