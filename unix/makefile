###############################################################################
# Makefile for SNAP and tools for *NIX GCC style platform
###############################################################################

# build type either debug or release
TYPE=release

WXDIR=../wxWidgets-2.8.12/build_gtk
WXCONFIG=$(WXDIR)/wx-config
WXFLAGS=`$(WXCONFIG) --cxxflags`
WXLINK=`$(WXCONFIG) --libs`
XLINK=-lX11

CC = g++
AR = ar
CP = cp
MD = -mkdir -p
RM = rm -f
LAPG = ../tools/lapg/lapg

ifeq ($(TYPE),debug)
LDFLAGS = -lm -lboost_regex -lefence
CCPARAM = -Wall -g -fpermissive -std=c++0x $(WXFLAGS)
MACROS = UNIX CHECKBLT
endif

ifeq ($(TYPE), release)
LDFLAGS = -lm -lboost_regex -s
CCPARAM = -Wall -O2 -fpermissive -std=c++0x $(WXFLAGS)
MACROS = NDEBUG UNIX 
endif

SRCDIR = ../src
DISTDIR = $(TYPE)/distrib
INSTALL = $(TYPE)/install
OBJ = $(TYPE)/obj
SNAPLIB = $(OBJ)/snaplib.a
WXUTILSLIB = $(OBJ)/wxutils.a
SNAPEXE = $(INSTALL)/snap
CONCORDEXE = $(INSTALL)/concord
SNAPSPECEXE = $(INSTALL)/snapspec
DAT2SITEEXE = $(INSTALL)/dat2site
SNAPLISTEXE = $(INSTALL)/snaplist
SNAPCONVEXE = $(INSTALL)/snapconv
SNAPGEOIDEXE = $(INSTALL)/snapgeoid
SNAPMERGEEXE = $(INSTALL)/snapmerge
SNAP_MANAGEREXE = $(INSTALL)/snap_manager
SNAPADJUSTEXE = $(INSTALL)/snapadjust
SNAPPLOTEXE = $(INSTALL)/snapplot

INCPATH = $(SRCDIR)/snaplib $(SRCDIR)/snaplib/util $(SRCDIR)/wxutils $(SRCDIR)/snapspec $(SRCDIR)/snap $(SRCDIR) $(SRCDIR)/snaplib/dbl4 $(SRCDIR)



CCFLAGS := $(CCPARAM) \
	   $(foreach INC,$(INCPATH),-I$(INC)) \
           $(foreach MACRO,$(MACROS),-D$(MACRO))

include ./makefile.inc

.PHONY: clean dirs

snap_all: makefile.inc makedirs $(SNAPEXE) $(CONCORDEXE) $(SNAPSPECEXE) $(DAT2SITEEXE) \
	$(SNAPLISTEXE) $(SNAPCONVEXE) $(SNAPGEOIDEXE) $(SNAPMERGEEXE) \
	$(SNAPLIB) support_files

snap_wx: snap_all $(SNAP_MANAGEREXE) $(SNAPPLOT)

all: snap_all

./makefile.inc:
	@python build_makefile_inc.py > makefile.inc

$(OBJ)/%.o: $(SRCDIR)/%.cpp
	@echo Compiling $<...
	$(CC) $(CCFLAGS) -c $< -o $@

$(OBJ)/%.o: $(SRCDIR)/%.c
	@echo Compiling $<...
	$(CC) $(CCFLAGS) -c $< -o $@

$(sort $(OBJDIRS)):
	$(MD) $@

makedirs: $(sort $(OBJDIRS))

$(SNAPEXE): makedirs $(SNAPLIB) $(SNAPOBJS)
	$(CC) $(CCFLAGS) $(SNAPOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPEXE)

$(CONCORDEXE): makedirs $(SNAPLIB) $(CONCORDOBJS)
	$(CC) $(CCFLAGS) $(CONCORDOBJS) $(SNAPLIB) $(LDFLAGS) -o $(CONCORDEXE)

$(SNAPSPECEXE): makedirs $(SNAPSPECOBJS) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAPSPECOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPSPECEXE)

$(DAT2SITEEXE): makedirs $(DAT2SITEOBJS) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(DAT2SITEOBJS) $(SNAPLIB) $(LDFLAGS) -o $(DAT2SITEEXE)

$(SNAPLISTEXE): makedirs $(SNAPLISTOBJS) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAPLISTOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPLISTEXE)

$(SNAPCONVEXE): makedirs $(SNAPCONVOBJS) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAPCONVOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPCONVEXE)

$(SNAPGEOIDEXE): makedirs $(SNAPGEOIDOBJS) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAPGEOIDOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPGEOIDEXE)

$(SNAPMERGEEXE): makedirs $(SNAPMERGEOBJS) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAPMERGEOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPMERGEEXE)

$(SNAP_MANAGEREXE): makedirs $(SNAP_MANAGEROBJS) $(SNAPLIB) $(WXUTILSLIB)
	$(CC) $(CCFLAGS) $(SNAP_MANAGEROBJS) $(SNAPLIB) $(WXUTILSLIB) $(WXLINK) $(XLINK) $(LDFLAGS) -o $(SNAP_MANAGEREXE)

$(SNAPADJUSTEXE): makedirs $(SNAPADJUSTOBJS) $(SNAPLIB) $(WXUTILSLIB)
	$(CC) $(CCFLAGS) $(SNAPADJUSTOBJS) $(SNAPLIB) $(WXUTILSLIB) $(WXLINK) $(XLINK) $(LDFLAGS) -o $(SNAPADJUSTEXE)

$(SNAPPLOTEXE): makedirs $(SNAPPLOTOBJS) $(SNAPLIB) $(WXUTILSLIB)
	$(CC) $(CCFLAGS) $(SNAPPLOTOBJS) $(SNAPLIB) $(WXUTILSLIB) $(WXLINK) $(XLINK) $(LDFLAGS) -o $(SNAPPLOTEXE)

$(SRCDIR)/snap_manager/scriptimp_parse.cpp: $(SRCDIR)/snap_manager/scriptimp_parse.syntax
	$(LAPG) $(SRCDIR)/snap_manager/scriptimp_parse.syntax $(SRCDIR)/snap_manager/scriptimp_parse.cpp

$(SNAPLIB): makedirs $(SNAPLIBOBJS)
	@echo Building snaplib
	$(AR) r $(SNAPLIB) $(SNAPLIBOBJS)

$(WXUTILSLIB): makedirs $(WXUTILSOBJS)
	@echo Building wxutils
	$(AR) r $(WXUTILSLIB) $(WXUTILSOBJS)

support_files: makedirs
	@echo Copying support files.
	mkdir -p $(INSTALL)/config
	$(CP) -r $(SRCDIR)/coordsys $(INSTALL)/config
	$(CP) -r $(SRCDIR)/snap/config/* $(INSTALL)/config
	$(CP) -r $(SRCDIR)/snapspec/config/* $(INSTALL)/config
	$(CP) -r $(SRCDIR)/snaplist/config/* $(INSTALL)/config
	$(CP) -r $(SRCDIR)/snap_manager/config/* $(INSTALL)/config
	$(CP) -r $(SRCDIR)/help/help $(INSTALL)

$(sort $(DISTDIR)):
	$(MD) $@

dist: snap_all $(sort $(DISTDIR))
	@echo Making distribution.
	$(CP) $(DISTFILES) $(DISTDIR)
	@( cd $(DISTDIR); \
	tar czf ../../snap-bin-$(TYPE).tar.gz *; )

clean:
	@echo Deleting object tree $(OBJ)...
	$(RM) -r $(OBJ)
	@echo Deleting built files
	$(RM) -r $(INSTALL)
	@echo Deleting makefile.inc
	$(RM) makefile.inc
