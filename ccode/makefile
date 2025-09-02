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

GUIEXTRAS := icon-512.png purpleclear58.png purpleflat58.png violetcirc58.png purpleclear116.png violetcirc116.png redleda36.png rl02pan.png procpan.png pdplogo.png

default: memtest.$(MACH) z11ctrl.$(MACH) z11dl.$(MACH) z11dz.$(MACH) z11dump.$(MACH) \
		z11ila.$(MACH) z11pc.$(MACH) z11pidp.$(MACH) z11rh.$(MACH) z11rl.$(MACH) \
		z11tm.$(MACH) z11xe.$(MACH) simtrace.$(MACH) absldr.lst \
	Z11GUI.jar libGUIZynqPage.$(MACH).so

lib.$(MACH).a: \
		disassem.$(MACH).o \
		pintable.$(MACH).o \
		readprompt.$(MACH).o \
		shmms.$(MACH).o \
		strprintf.$(MACH).o \
		tapelib.$(MACH).o \
		tclmain.$(MACH).o \
		z11util.$(MACH).o
	rm -f lib.$(MACH).a
	ar rc $@ $^

../macro11/macro11.$(MACH): ../macro11/Makefile
	rm -f ../macro11/*.o
	$(MAKE) -C ../macro11
	mv ../macro11/macro11 ../macro11/macro11.$(MACH)

../macro11/Makefile:
	git clone https://gitlab.com/Rhialto/macro11.git ../macro11

../verisim/verisim.$(MACH).a:
	$(MAKE) -C ../verisim verisim.$(MACH).a

%.$(MACH): %.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

%.$(MACH).o: %.cc *.h
	$(GPP) -c -o $@ $<

%.lst:	%.mac ../macro11/macro11.$(MACH)
	../macro11/macro11.$(MACH) -o x.obj -l $@ $<
	rm -f x.obj

Z11GUI.jar: Z11GUI.java GUIZynqPage.class $(GUIEXTRAS)
	rm -f Z11GUI.class Z11GUI\$$*.class Z11GUI.jar
	CLASSPATH=. javac Z11GUI.java
	jar cf Z11GUI.jar Z11GUI*.class GUIZynqPage.class $(GUIEXTRAS)
	rm -f Z11GUI.class Z11GUI\$$*.class

libGUIZynqPage.$(MACH).so: GUIZynqPage.cc GUIZynqPage.h $(LIBS)
	$(GPP) -shared -o libGUIZynqPage.$(MACH).so -I/opt/jdk/include -I/opt/jdk/include/linux GUIZynqPage.cc $(LIBS) $(LNKFLG)

GUIZynqPage.class GUIZynqPage.h: GUIZynqPage.java
	rm -f GUIZynqPage.class GUIZynqPage.h
	CLASSPATH=. javac -h . GUIZynqPage.java

