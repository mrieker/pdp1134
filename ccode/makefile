CFLAGS ?= -O2 -Werror
MACH   := $(shell uname -m)
TCLINC := -I$(shell ./tclinc.sh)
GPP    := g++ $(CFLAGS) $(TCLINC) -std=c++11 -g -Wall -fPIC
LNKFLG := -lpthread -lreadline -lrt -l$(shell ./tcllib.sh)

ifeq ($(MACH),armv7l)
	LNKFLG := $(LNKFLG) -latomic
endif

LIBS = lib.$(MACH).a

default: z11ctrl.$(MACH) z11dl.$(MACH) z11dump.$(MACH) z11ila.$(MACH) z11pr.$(MACH)

lib.$(MACH).a: \
		cmd_pin.$(MACH).o \
		readprompt.$(MACH).o \
		tclmain.$(MACH).o \
		z11util.$(MACH).o
	rm -f lib.$(MACH).a
	ar rc $@ $^

z11ctrl.$(MACH): z11ctrl.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z11dl.$(MACH): z11dl.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z11dump.$(MACH): z11dump.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z11ila.$(MACH): z11ila.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z11pr.$(MACH): z11pr.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

%.$(MACH).o: %.cc *.h
	$(GPP) -c -o $@ $<

