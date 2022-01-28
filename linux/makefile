###############################################################################
# Makefile for SNAP and tools for *NIX GCC style platform
###############################################################################

# build type either debug or release
type=release
ifdef SNAP_BUILD_DIR
builddir=$(SNAP_BUILD_DIR)
else
builddir=$(type)
endif

COORDSYSDIR=/usr/share/linz/coordsys
INSTALLDIR=${DESTDIR}/usr/share/linz/snap
BINDIR=${DESTDIR}/usr/bin
INCFILE=makefile.inc
SNAPVER=`cat ../src/VERSION`

WXCONFIG=wx-config
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
LDFLAGS = -lm -lboost_regex -lboost_filesystem -lpthread 
#LDFLAGS = -lm -lboost_regex -lpthread  -lefence
LDWXFLAGS = -lm -lboost_regex -lboost_filesystem
CCPARAM = -Wall -g -fpermissive -std=c++0x $(WXFLAGS)
MACROS = UNIX CHECKBLT
TESTFLAGS = 
UPDATEVERSION =
SNAPPACKAGES = devel_package

else ifeq ($(type), release)
LDFLAGS = -lm -lboost_regex -lboost_filesystem -lpthread -s
LDWXFLAGS = -lm -lboost_regex -lboost_filesystem -s
#CCPARAM = -Wall -O2 -fpermissive -std=c++0x $(WXFLAGS)
CCPARAM = -Wall -Ofast -fpermissive -std=c++0x $(WXFLAGS)
MACROS = NDEBUG UNIX 
TESTFLAGS = -r
UPDATEVERSION = update_version_date
SNAPPACKAGES = 

else
MD=echo
endif

SRCDIR = ../src
INSTALL = $(builddir)/install
OBJ = $(builddir)/obj
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

-include $(INCFILE)

.PHONY: clean dirs

all: snap_all

help:
	@echo "make type=(release|debug) (all|snap_cmd|test|install|package)"
	@echo ""
	@echo "release builds optimised version for install/packaging"
	@echo "install/package require all changes to be committed"
	@echo "package requires changelog to match version (src/VERSION)"

installable: 
	@if ! git diff --quiet HEAD; then echo "**************************"; echo "ABORTED: Current files not commited"; exit 1; fi
	
ifeq ($(type), release)
install: installable all
	mkdir -p $(INSTALLDIR)
	mkdir -p $(BINDIR)
	cp -r $(INSTALL)/* $(INSTALLDIR)

package: installable
	@if  ! ( head -1 debian/changelog | grep -q "($(SNAPVER)-" ) ; then echo "***************************************"; echo "ABORTED: changelog version does not match $(SNAPVER)"; exit 1; fi
	@echo "Building debian package"
	SNAP_BUILD_DIR=$(builddir) debuild -uc -us -b
else
install: 
	@echo "Can only build installation for release version"

package: 
	@echo "Can only build package for release version"
endif

# All programs including wxWidgets components
snap_all: snap_cmd snap_manager snapadjust snapplot $(SNAPPACKAGES)

# Command line programs only
snap_cmd: $(UPDATEVERSION) $(INCFILE) snap concord snapspec dat2site snaplist snapconv snapgeoid snapmerge site2gps
	
snap: $(SNAPEXE) support_files

concord: $(CONCORDEXE) coordsys

snapspec: $(SNAPSPECEXE) support_files

dat2site: $(DAT2SITEEXE) support_files

snaplist: $(SNAPLISTEXE) support_files

snapconv: $(SNAPCONVEXE) support_files

snapgeoid: $(SNAPGEOIDEXE) support_files

snapmerge: $(SNAPMERGEEXE) support_files

site2gps: $(SITE2GPSEXE) support_files

snap_manager: $(SNAP_MANAGEREXE) support_files help_files

snapplot: $(SNAPPLOTEXE) support_files help_files

snapadjust: $(SNAPADJUSTEXE) support_files


# Test (command line programs only)
test: snap_cmd
	../regression_tests/testall.pl $(TESTFLAGS)

update_version_date:
	touch `grep -rl GETVERSION_SET_PROGRAM_DATE ../src`

rebuild_makefile: $(SRCDIR)/snap_manager/scriptimp_parse.cpp
	@python build_makefile_inc.py > $(INCFILE)

$(INCFILE): $(SRCDIR)/snap_manager/scriptimp_parse.cpp
	@python3 build_makefile_inc.py > $(INCFILE)

$(OBJ)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p `dirname $@` 
	@echo Compiling $<...
	$(CC) $(CCFLAGS) -c $< -o $@

$(OBJ)/%.o: $(SRCDIR)/%.c
	@mkdir -p `dirname $@` 
	@echo Compiling $<...
	$(CC) $(CCFLAGS) -c $< -o $@

$(SNAPEXE): $(SNAPLIB) $(SNAPOBJS)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPEXE)

$(CONCORDEXE): $(SNAPLIB) $(CONCORDOBJS)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(CONCORDOBJS) $(SNAPLIB) $(LDFLAGS) -o $(CONCORDEXE)

$(SNAPSPECEXE): $(SNAPSPECOBJS) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPSPECOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPSPECEXE)

$(DAT2SITEEXE): $(DAT2SITEOBJS) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(DAT2SITEOBJS) $(SNAPLIB) $(LDFLAGS) -o $(DAT2SITEEXE)

$(SNAPLISTEXE): $(SNAPLISTOBJS) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPLISTOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPLISTEXE)

$(SNAPCONVEXE): $(SNAPCONVOBJS) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPCONVOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPCONVEXE)

$(SNAPGEOIDEXE): $(SNAPGEOIDOBJS) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPGEOIDOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPGEOIDEXE)

$(SNAPMERGEEXE): $(SNAPMERGEOBJS) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPMERGEOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SNAPMERGEEXE)

$(SITE2GPSEXE): $(SITE2GPSOBJS) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SITE2GPSOBJS) $(SNAPLIB) $(LDFLAGS) -o $(SITE2GPSEXE)

$(SNAP_MANAGEREXE): $(SNAP_MANAGEROBJS) $(WXUTILSLIB) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAP_MANAGEROBJS) $(WXUTILSLIB) $(SNAPLIB) $(WXLINK) $(XLINK) $(LDWXFLAGS) -o $(SNAP_MANAGEREXE)

$(SNAPADJUSTEXE): $(SNAPADJUSTOBJS) $(WXUTILSLIB) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPADJUSTOBJS) $(WXUTILSLIB) $(SNAPLIB) $(WXLINK) $(XLINK) $(LDWXFLAGS) -o $(SNAPADJUSTEXE)

$(SNAPPLOTEXE): $(SNAPPLOTOBJS) $(WXUTILSLIB) $(SNAPLIB)
	@mkdir -p `dirname $@` 
	$(CC) $(CCFLAGS) $(SNAPPLOTOBJS) $(WXUTILSLIB) $(SNAPLIB) $(WXLINK) $(XLINK) $(LDWXFLAGS) -o $(SNAPPLOTEXE)

$(LAPG): 
	cd $(LAPGSRCDIR) && make

$(SRCDIR)/snap_manager/scriptimp_parse.cpp: $(SRCDIR)/snap_manager/scriptimp_parse.syntax $(LAPG)
	$(LAPG) $(SRCDIR)/snap_manager/scriptimp_parse.syntax $(SRCDIR)/snap_manager/scriptimp_parse.cpp

$(SNAPLIB): $(SNAPLIBOBJS)
	@echo Building snaplib
	$(RM) $(SNAPLIB)
	$(AR) r $(SNAPLIB) $(SNAPLIBOBJS)

$(WXUTILSLIB): $(WXUTILSOBJS)
	@echo Building wxutils
	$(AR) r $(WXUTILSLIB) $(WXUTILSOBJS)

coordsys:
	@echo Installing coordinate system link
	@mkdir -p $(INSTALL)/config
	@rm -rf $(INSTALL)/config/coordsys
	@ln -s -T -f $(COORDSYSDIR) $(INSTALL)/config/coordsys

support_files: coordsys
	@echo Updating support files.
	@mkdir -p $(INSTALL)/config
	@$(CP) -r -p $(SRCDIR)/perl $(INSTALL)/config
	@$(CP) -r -p $(SRCDIR)/snap/config/* $(INSTALL)/config
	@$(CP) -r -p $(SRCDIR)/snapspec/config/* $(INSTALL)/config
	@$(CP) -r -p $(SRCDIR)/snaplist/config/* $(INSTALL)/config
	@$(CP) -r -p $(SRCDIR)/snap_manager/config/* $(INSTALL)/config
	@$(CP) $(SRCDIR)/VERSION $(INSTALL)/
	@if which git; then git rev-parse --short HEAD > $(INSTALL)/VERSIONID; fi

help_files: helpfile 

helpfile:
	(cd $(SRCDIR)/help && ./build_help.sh)
	$(CP) -r $(SRCDIR)/help/help $(INSTALL)

devel_package: 
	mkdir -p $(INSTALL)/config/package
	cp -r $(SRCDIR)/packages/devel $(INSTALL)/config/package/

# Previous attempt at help...
wxhelpdir:
	(cd $(SRCDIR)/help && ./build_wxhelp_map.pl)
	mkdir -p $(INSTALL)/snaphelp
	$(CP) -r $(SRCDIR)/help/help/* $(INSTALL)/snaphelp/
	$(CP) -r $(SRCDIR)/help/wxhelp.map $(INSTALL)/snaphelp/

clean:
	@echo Deleting object tree $(OBJ)...
	$(RM) -r $(OBJ)
	@echo Deleting built files
	$(RM) -r $(INSTALL)
	
