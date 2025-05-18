CFLAGS ?= -O2 -Werror
MACH   := $(shell uname -m)
TCLINC := -I$(shell ./tclinc.sh)
LNKFLG := -lpthread -lreadline -lrt -l$(shell ./tcllib.sh)
LIBS   := lib.$(MACH).a

ifeq ($(MACH),armv7l)
	LNKFLG := $(LNKFLG) -latomic
endif
ifeq ($(MACH),x86_64)
	CFLAGS := $(CFLAGS) -DVERISIM
	LIBS   := $(LIBS) ../verisim/verisim.$(MACH).a
endif

GPP    := g++ $(CFLAGS) $(TCLINC) -std=c++11 -g -Wall -fPIC

default: z11ctrl.$(MACH) z11dl.$(MACH) z11dump.$(MACH) z11ila.$(MACH) z11pr.$(MACH) z11rl.$(MACH)

lib.$(MACH).a: \
		cmd_pin.$(MACH).o \
		readprompt.$(MACH).o \
		tclmain.$(MACH).o \
		z11util.$(MACH).o
	rm -f lib.$(MACH).a
	ar rc $@ $^

../verisim/verisim.$(MACH).a:
	$(MAKE) -C ../verisim verisim.$(MACH).a

%.$(MACH): %.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

%.$(MACH).o: %.cc *.h
	$(GPP) -c -o $@ $<

