###############################################################################
# Makefile for SNAP and tools for *NIX GCC style platform
###############################################################################

# build type either debug or release
type=release
# Force recompile of all version dates
ifeq ($(type),release)
versiondate=yes
else
versiondate=no
endif

COORDSYSDIR=/usr/share/linz/coordsys
INSTALLDIR=${DESTDIR}/usr/share/linz/snap
BINDIR=${DESTDIR}/usr/bin
SNAPVER=`cat ../src/VERSION`

WXDIR=../wx/wxWidgets-2.8.12/build/gtk
WXCONFIG=$(WXDIR)/wx-config
WXFLAGS=`$(WXCONFIG) --cxxflags`
WXLINK=`$(WXCONFIG) --libs`
XLINK=-lX11

CC = g++
AR = ar
CP = cp
MD = mkdir -p
RM = rm -f
LAPG = ../tools/lapg/lapg
LAPGSRCDIR=../tools/lapg/src

ifeq ($(type),debug)
#LDFLAGS = -lm -lboost_regex
LDFLAGS = -lm -lboost_regex -lpthread -lefence
LDWXFLAGS = -lm -lboost_regex 
CCPARAM = -Wall -g -fpermissive -std=c++0x $(WXFLAGS)
MACROS = UNIX CHECKBLT

else ifeq ($(type), release)
LDFLAGS = -lm -lboost_regex -lpthread -s
LDWXFLAGS = -lm -lboost_regex -s
#CCPARAM = -Wall -O2 -fpermissive -std=c++0x $(WXFLAGS)
CCPARAM = -Wall -Ofast -fpermissive -std=c++0x $(WXFLAGS)
MACROS = NDEBUG UNIX 

else
MD=echo
endif

SRCDIR = ../src
DISTDIR = $(type)/distrib
INSTALL = $(type)/install
OBJ = $(type)/obj
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
SITE2GPSEXE = $(INSTALL)/site2gps
SNAP_MANAGEREXE = $(INSTALL)/snap_manager
SNAPADJUSTEXE = $(INSTALL)/snapadjust
SNAPPLOTEXE = $(INSTALL)/snapplot

INCPATH = $(SRCDIR)/snaplib $(SRCDIR)/snaplib/util $(SRCDIR)/wxutils $(SRCDIR)/snapspec $(SRCDIR)/snap $(SRCDIR) $(SRCDIR)/snaplib/dbl4 $(SRCDIR)



CCFLAGS := $(CCPARAM) \
	   $(foreach INC,$(INCPATH),-I$(INC)) \
           $(foreach MACRO,$(MACROS),-D$(MACRO))

include ./makefile.inc

.PHONY: clean dirs

all: snap_all

ifeq ($(type), release)
install: all
	@if ! git diff --quiet HEAD; then echo "**************************"; echo "ABORTED: Current files not commited"; exit 1; fi
	mkdir -p $(INSTALLDIR)
	mkdir -p $(BINDIR)
	cp -r $(INSTALL)/* $(INSTALLDIR)
	git rev-parse
else
install: 
	echo "Can only build installation for release version"
endif

package: 
	@if  ! ( head -1 debian/changelog | grep -q "($(SNAPVER)-" ) ; then echo "***************************************"; echo "ABORTED: changelog version does not match $(SNAPVER)"; exit 1; fi
	@echo "Building debian package"
	debuild -uc -us -b

# All programs including wxWidgets components
snap_all: snap_cmd $(SNAP_MANAGEREXE) $(SNAPADJUSTEXE) $(SNAPPLOTEXE)

# Command line programs only
snap_cmd: version makefile.inc makedirs $(SNAPEXE) $(CONCORDEXE) $(SNAPSPECEXE) $(DAT2SITEEXE) \
	$(SNAPLISTEXE) $(SNAPCONVEXE) $(SNAPGEOIDEXE) $(SNAPMERGEEXE) $(SITE2GPSEXE) \
	$(SNAPLIB) support_files

version:
ifeq ($(versiondate), no)
	echo "Version date info not updated"
else
	touch ../src/*/version.c
endif

./makefile.inc: $(SRCDIR)/snap_manager/scriptimp_parse.cpp
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

$(SITE2GPSEXE): makedirs $(SITE2GPSOBJS) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SITE2GPSOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SITE2GPSEXE)

$(SNAP_MANAGEREXE): makedirs $(SNAP_MANAGEROBJS) $(WXUTILSLIB) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAP_MANAGEROBJS) $(WXUTILSLIB) $(SNAPLIB) $(WXLINK) $(XLINK) $(LDWXFLAGS) -o $(SNAP_MANAGEREXE)

$(SNAPADJUSTEXE): makedirs $(SNAPADJUSTOBJS) $(WXUTILSLIB) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAPADJUSTOBJS) $(WXUTILSLIB) $(SNAPLIB) $(WXLINK) $(XLINK) $(LDWXFLAGS) -o $(SNAPADJUSTEXE)

$(SNAPPLOTEXE): makedirs $(SNAPPLOTOBJS) $(WXUTILSLIB) $(SNAPLIB)
	$(CC) $(CCFLAGS) $(SNAPPLOTOBJS) $(WXUTILSLIB) $(SNAPLIB) $(WXLINK) $(XLINK) $(LDWXFLAGS) -o $(SNAPPLOTEXE)

$(LAPG): 
	cd $(LAPGSRCDIR) && make

$(SRCDIR)/snap_manager/scriptimp_parse.cpp: $(SRCDIR)/snap_manager/scriptimp_parse.syntax
	$(LAPG) $(SRCDIR)/snap_manager/scriptimp_parse.syntax $(SRCDIR)/snap_manager/scriptimp_parse.cpp

$(SNAPLIB): makedirs $(SNAPLIBOBJS)
	@echo Building snaplib
	$(RM) $(SNAPLIB)
	$(AR) r $(SNAPLIB) $(SNAPLIBOBJS)

$(WXUTILSLIB): makedirs $(WXUTILSOBJS)
	@echo Building wxutils
	$(AR) r $(WXUTILSLIB) $(WXUTILSOBJS)

support_files: makedirs helpfile wxhelpdir
	@echo Copying support files.
	mkdir -p $(INSTALL)/config
	rm -rf $(INSTALL)/config/coordsys
	ln -s -T -f $(COORDSYSDIR) $(INSTALL)/config/coordsys
	$(CP) -r -p $(SRCDIR)/perl $(INSTALL)/config
	$(CP) -r -p $(SRCDIR)/snap/config/* $(INSTALL)/config
	$(CP) -r -p $(SRCDIR)/snapspec/config/* $(INSTALL)/config
	$(CP) -r -p $(SRCDIR)/snaplist/config/* $(INSTALL)/config
	$(CP) -r -p $(SRCDIR)/snap_manager/config/* $(INSTALL)/config
	$(CP) $(SRCDIR)/VERSION $(INSTALL)/
	git rev-parse --short HEAD > $(INSTALL)/VERSIONID

helpfile:
	(cd $(SRCDIR)/help && ./build_snaphelp.sh)
	$(CP) -r $(SRCDIR)/help/snaphelp.zip $(INSTALL)

wxhelpdir:
	(cd $(SRCDIR)/help && ./build_wxhelp_map.pl)
	mkdir -p $(INSTALL)/snaphelp
	$(CP) -r $(SRCDIR)/help/help/* $(INSTALL)/snaphelp/
	$(CP) -r $(SRCDIR)/help/wxhelp.map $(INSTALL)/snaphelp/

$(sort $(DISTDIR)):
	$(MD) $@

dist: snap_all $(sort $(DISTDIR))
	@echo Making distribution.
	$(CP) $(DISTFILES) $(DISTDIR)
	@( cd $(DISTDIR); \
	tar czf ../../snap-bin-$(type).tar.gz *; )

clean:
	@echo Deleting object tree $(OBJ)...
	$(RM) -r $(OBJ)
	@echo Deleting built files
	$(RM) -r $(INSTALL)
	@echo Deleting makefile.inc
	$(RM) makefile.inc
