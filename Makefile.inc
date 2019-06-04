
LIBRARY_NAME    := keydetector

SOURCES         := \
                src/KeyDetector.cpp \
                src/KeyDetectorDaschuer.cpp \
                src/KeyDetectorQM.cpp

HEADERS         := \
                keydetector/KeyDetector.h \
		src/KeyDetectorIface.h \
		src/KeyDetectorDaschuer.h \
		src/KeyDetectorQM.h

##  Normally you should not edit anything below this line

QM_DSP_DIR	?= ../qm-dsp

CFLAGS		:= $(ARCHFLAGS) $(CFLAGS)
CXXFLAGS	:= $(CFLAGS) -I. -I$(QM_DSP_DIR) $(CXXFLAGS)

LIB_EXT         ?= .a
CXX 		?= g++
CC 		?= gcc
AR              ?= ar
RANLIB          ?= ranlib

LIBRARY         := $(LIB_PREFIX)$(LIBRARY_NAME)$(LIB_EXT)

OBJECTS 	:= $(SOURCES:.cpp=.o)
OBJECTS 	:= $(OBJECTS:.c=.o)

$(LIBRARY): $(OBJECTS) 
	$(AR) cr $@ $^
	$(RANLIB) $@

$(OBJECTS): $(HEADERS)

.PHONY: plugin
plugin:	$(LIBRARY)
	$(MAKE) -C vamp -f Makefile$(MAKEFILE_EXT)

clean:
	rm -f $(OBJECTS)

distclean:	clean
	rm -f $(LIBRARY)

depend:
	makedepend -Y -fMakefile.inc $(SOURCES) $(HEADERS)

# DO NOT DELETE

src/KeyDetectorQM.o: keydetector/KeyDetector.h
src/KeyDetectorQM.o: src/KeyDetectorIface.h
