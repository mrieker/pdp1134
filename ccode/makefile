CFLAGS ?= -O2 -Werror
MACH   := $(shell uname -m)
TCLINC := -I$(shell ./tclinc.sh)
LNKFLG := -lpthread -lreadline -lrt -l$(shell ./tcllib.sh)
LIBS   := lib.$(MACH).a

ifeq ($(MACH),armv7l)
	LNKFLG := $(LNKFLG) -latomic
endif
ifeq ($(MACH),x86_64)
	CFLAGS := $(CFLAGS) -DSIMRPAGE
	LIBS   := $(LIBS) ../simmer/simrpage.$(MACH).o
endif

GPP    := g++ $(CFLAGS) $(TCLINC) -std=c++11 -g -Wall -fPIC

default: memtest.$(MACH) z11ctrl.$(MACH) z11dl.$(MACH) z11dump.$(MACH) z11ila.$(MACH) z11pr.$(MACH) z11rl.$(MACH) \
	GUI.jar libGUIZynqPage.$(MACH).so

lib.$(MACH).a: \
		cmd_pin.$(MACH).o \
		disassem.$(MACH).o \
		readprompt.$(MACH).o \
		strprintf.$(MACH).o \
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

GUI.jar: GUI.java GUIZynqPage.h purplecirc52.png violetcirc52.png purplecirc117.png violetcirc117.png
	rm -f GUI.class GUI\$$*.class GUI.jar
	CLASSPATH=. javac GUI.java
	jar cf GUI.jar GUI*.class purplecirc52.png violetcirc52.png purplecirc117.png violetcirc117.png
	rm -f GUI.class GUI\$$*.class

libGUIZynqPage.$(MACH).so: GUIZynqPage.cc GUIZynqPage.h $(LIBS)
	$(GPP) -shared -o libGUIZynqPage.$(MACH).so -I/opt/jdk/include -I/opt/jdk/include/linux GUIZynqPage.cc $(LIBS) $(LNKFLG)

GUIZynqPage.h: GUIZynqPage.java
	rm -f GUIZynqPage.class GUIZynqPage.h
	CLASSPATH=. javac -h . GUIZynqPage.java

