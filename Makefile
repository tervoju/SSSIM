CXX=g++ 
INCLUDES=
DEFINITIONS=-DFREEGLUT
DIRTY_FLAGS=-ansi -Wall -MMD -std=c++11 
FLAGS=$(DIRTY_FLAGS) #-Weffc++
DEBUG=-g -O2
GUI_DEBUG=-O0

LD=$(CXX) -L obj
LIBS=-lnetcdf_c++ -lnetcdf -lGLU -lglut -lGL -lsvl -lm -lpthread -lconfig++

BIN_PATH=bin

COMMON=sssim util-math util-convert udp 

ENV=$(COMMON) util-trigger util-mkdirp env-sea env-float env-time env-server env-clients env-gui satmsg-fmt satmsg-data float-structs

CLI=sssim udp

GUI=$(COMMON) gui-sea
CLIENT=$(COMMON) client-ctrl client-init client-print client-socket client-time util-msgqueue satmsg-fmt satmsg-data satmsg-modem soundmsg-fmt sssim-structs util-leastsquares ctd-tracker ctd-estimate sat-client gps-client float-util float-structs
BASE=$(CLIENT) base-config
FLOAT=$(CLIENT) dive-ctrl dive-init float-fsm util-trigger util-tsdouble soundmsg-modem float-group kalman_filter

.SECONDEXPANSION:

OBJSELF=obj/$$(subst bin/,,$$@).opp
DIRTY_OBJSELF=obj/$$@.dopp
OBJENV=$(addprefix obj/,$(addsuffix .opp,$(ENV)))
OBJCLI=$(addprefix obj/,$(addsuffix .opp,$(CLI)))
OBJGUI=$(addprefix obj/,$(addsuffix .opp,$(GUI)))
OBJBASE=$(addprefix obj/,$(addsuffix .opp,$(BASE)))
OBJFLOAT=$(addprefix obj/,$(addsuffix .opp,$(FLOAT)))

## local changes for directories, g++ wrappers, etc. (optional)
#-include Makefile.local

## To include gui screenshot support, set SCREENSHOTS=1
## note: requires dev version of libgd2 package
SCREENSHOTS=0

ifeq ($(SCREENSHOTS),1)
LIBSGD=-lgd -lpng -lz -ljpeg -lfreetype
DEFINITIONS+= -DINC_LIBGD
endif


.PHONY: all clean realclean

all: env cli gui base float

clean:
	rm -f bin/* obj/*opp

realclean:
	rm -f bin/* obj/* *~

env: $(BIN_PATH)/env ;
$(BIN_PATH)/env: $(OBJSELF) $(OBJENV)
	@echo "  [LD] $@"
	@$(LD) -o $@ $^ $(LIBS) -lpthread

cli: $(BIN_PATH)/cli ;
$(BIN_PATH)/cli: $(OBJSELF) $(OBJCLI)
	@echo "  [LD] $@"
	@$(LD) -o $@ $^ $(LIBS) $(LIBSGD)

gui: $(BIN_PATH)/gui ;
$(BIN_PATH)/gui: $(OBJSELF) $(OBJGUI)
	@echo "  [LD] $@"
	@$(LD) -o $@ $^ $(LIBS) $(LIBSGD)

base: $(BIN_PATH)/base ;
$(BIN_PATH)/base: $(OBJSELF) $(OBJBASE)
	@echo "  [LD] $@"
	@$(LD) -o $@ $^ $(LIBS) -lpthread

float: $(BIN_PATH)/float ;
$(BIN_PATH)/float: $(OBJSELF) $(OBJFLOAT)
	@echo "  [LD] $@"
	@$(LD) -o $@ $^ $(LIBS) -lpthread

#.DEFAULT:
#	$(CXX) $(FLAGS) $(DEBUG) $(INCLUDES) $(DEFINITIONS) -c -o obj/$@.opp src/$@.cpp
#	$(LD) -o $@ obj/$@.opp $(LIBS)


## separate due to failing at -O1 and above: something to do with pov_tgt
obj/gui.opp: src/gui.cpp
	@echo "  [C++] $@"
	@$(CXX) $(FLAGS) $(GUI_DEBUG) $(INCLUDES) $(DEFINITIONS) -c -o $@ $<

## Below this is dependency generation stuff, no need to change.

obj/%.opp: $(addprefix src/,%.cpp)
	@echo "  [C++] $@"
	@if [ ! -d obj ]; then mkdir obj; fi
	@$(CXX) $(FLAGS) $(DEBUG) $(INCLUDES) $(DEFINITIONS) -c -o $@ $<

obj/%.dopp: $(addprefix src/,%.cpp)
	@echo "  [C++] $@"
	@if [ ! -d obj ]; then mkdir obj; fi
	@$(CXX) $(DIRTY_FLAGS) $(DEBUG) $(INCLUDES) $(DEFINITIONS) -c -o $@ $<

## don't try to re-make Makefile.local if missing
Makefile.local: ;

## Include dependency rules
-include obj/*.d
