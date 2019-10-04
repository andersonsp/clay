# --------------------------------------------------------------------------
#
# Tiny C Compiler Makefile
#

ifndef TOP
 TOP = .
 INCLUDED = no
endif

include $(TOP)/config.mak

ifeq (-$(CC)-$(GCC_MAJOR)-$(findstring $(GCC_MINOR),56789)-,-gcc-4--)
 CFLAGS += -D_FORTIFY_SOURCE=0
endif

LIBTCC = libtcc.a
LIBTCC1 = libtcc1.a
LIBS =
CFLAGS += -I$(TOP)
CFLAGS += $(CPPFLAGS)
VPATH = $(TOPSRC)
OSFLAG = linux

ifdef CONFIG_WIN32
	ifneq ($(CONFIG_static),yes)
		LIBTCC = libtcc$(DLLSUF)
		LIBTCCDEF = libtcc.def
	endif
	CFGWIN = -win
	NATIVE_TARGET = $(ARCH)-win$(if $(findstring arm,$(ARCH)),ce,32)
else
 	LIBS=-lm
 	ifneq ($(CONFIG_ldl),no)
  		LIBS+=-ldl
 	endif
 	# make libtcc as static or dynamic library?
 	ifeq ($(CONFIG_static),no)
  		LIBTCC=libtcc$(DLLSUF)
 	endif
 	CFGWIN =-unx
 	NATIVE_TARGET = $(ARCH)
 	ifdef CONFIG_OSX
  		NATIVE_TARGET = $(ARCH)-osx
  		LDFLAGS += -flat_namespace -undefined warning -rpath @executable_path/
  		export MACOSX_DEPLOYMENT_TARGET := 10.6
  	else
		LDFLAGS += -Wl,-rpath,\$$ORIGIN
 	endif
endif

# run local version of tcc with local libraries and includes
TCCFLAGS-unx = -B$(TOP) -I$(TOPSRC)/include -I$(TOPSRC) -I$(TOP)
TCCFLAGS-win = -B$(TOPSRC)/win32 -I$(TOPSRC)/include -I$(TOPSRC) -I$(TOP) -L$(TOP)

TCCFLAGS = $(TCCFLAGS$(CFGWIN))
TCC = $(TOP)/tcc$(EXESUF) $(TCCFLAGS)
ifdef CONFIG_OSX
 	TCCFLAGS += -D_ANSI_SOURCE

 	OSFLAG=macos
else ifdef CONFIG_WIN32
	OSFLAG=windows
endif

CONFIG_$(ARCH) = yes
NATIVE_DEFINES_$(CONFIG_i386) += -DTCC_TARGET_I386
NATIVE_DEFINES_$(CONFIG_x86_64) += -DTCC_TARGET_X86_64
NATIVE_DEFINES_$(CONFIG_WIN32) += -DTCC_TARGET_PE
NATIVE_DEFINES_$(CONFIG_OSX) += -DTCC_TARGET_MACHO
NATIVE_DEFINES_$(CONFIG_selinux) += -DHAVE_SELINUX
NATIVE_DEFINES += $(NATIVE_DEFINES_yes)

ifeq ($(INCLUDED),no)
# --------------------------------------------------------------------------
# running top Makefile

PROGS = tcc$(EXESUF) kiln$(EXESUF)
TCCLIBS = $(LIBTCC1) $(LIBTCC) $(LIBTCCDEF)
TCCDOCS = tcc.1 tcc-doc.html tcc-doc.info

all: $(PROGS) $(TCCLIBS) # $(TCCDOCS)

ifdef CONFIG_cross
all:
	@echo "Cross disabled" && exit 1
endif

# --------------------------------------------

T = $(or $(CROSS_TARGET),$(NATIVE_TARGET),unknown)
X = $(if $(CROSS_TARGET),$(CROSS_TARGET)-)

DEF-x86_64      = -DTCC_TARGET_X86_64
DEF-x86_64-win32= -DTCC_TARGET_PE -DTCC_TARGET_X86_64
DEF-x86_64-osx  = -DTCC_TARGET_MACHO -DTCC_TARGET_X86_64
DEF-$(NATIVE_TARGET) = $(NATIVE_DEFINES)

DEFINES += $(DEF-$T) $(DEF-all)
DEFINES += $(if $(ROOT-$T),-DCONFIG_SYSROOT="\"$(ROOT-$T)\"")
DEFINES += $(if $(CRT-$T),-DCONFIG_TCC_CRTPREFIX="\"$(CRT-$T)\"")
DEFINES += $(if $(LIB-$T),-DCONFIG_TCC_LIBPATHS="\"$(LIB-$T)\"")
DEFINES += $(if $(INC-$T),-DCONFIG_TCC_SYSINCLUDEPATHS="\"$(INC-$T)\"")
DEFINES += $(DEF-$(or $(findstring win,$T),unx))

ifneq ($(X),)
	ifeq ($(CONFIG_WIN32),yes)
		DEF-win += -DTCC_LIBTCC1="\"$(X)libtcc1.a\""
		DEF-unx += -DTCC_LIBTCC1="\"lib/$(X)libtcc1.a\""
	else
		DEF-all += -DTCC_LIBTCC1="\"$(X)libtcc1.a\""
		DEF-win += -DCONFIG_TCCDIR="\"$(tccdir)/win32\""
	endif
endif

# include custom configuration (see make help)
-include config-extra.mak

CORE_FILES = tcc.o tcctools.o libtcc.o tccpp.o tccgen.o tccelf.o tccasm.o tccrun.o
CORE_FILES += tcc.h libtcc.h tcctok.h

x86_64_FILES = $(CORE_FILES) x86_64-gen.o x86_64-link.o i386-asm.o x86_64-asm.h
x86_64-win32_FILES = $(x86_64_FILES) tccpe.o
x86_64-osx_FILES = $(x86_64_FILES)

# libtcc sources
LIBTCC_SRC = $(filter-out tcc.o kiln.o tcctools.o %.h, $($T_FILES))

# ifeq ($(ONE_SOURCE),yes)
# LIBTCC_OBJ = $(X)libtcc.o
# LIBTCC_INC = $($T_FILES) config.h
# TCC_FILES = $(X)tcc.o
# tcc.o : DEFINES += -DONE_SOURCE=0
# else
LIBTCC_OBJ = $(LIBTCC_SRC)
LIBTCC_INC = $(patsubst %.h, src/%.h, $(filter %.h, $($T_FILES))) config.h
kiln.o tcc.o $(LIBTCC_OBJ):	DEFINES += -DONE_SOURCE=0
# endif

# target specific object rule
%.o: src/%.c $(LIBTCC_INC)
	$(CC) -o $@ -c $< $(DEFINES) $(CFLAGS)

# additional dependencies
tcc.o : src/tcctools.c

# Host Tiny C Compiler
tcc$(EXESUF): tcc.o $(LIBTCC)
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

# kiln executable
kiln$(EXESUF): kiln.o $(LIBTCC)
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

# static libtcc library
# libtcc.a: $(LIBTCC_OBJ)
# 	$(AR) rcs $@ $^

# dynamic libtcc library
libtcc.so: $(LIBTCC_OBJ)
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LDFLAGS)

libtcc.so: CFLAGS+=-fPIC
libtcc.so: LDFLAGS+=-fPIC

libtcc.dylib: $(LIBTCC_OBJ)
	$(CC) -dynamiclib -install_name @rpath/$@ -o $@ $^ $(LDFLAGS)

libtcc.dylib: CFLAGS+=-fPIC
libtcc.dylib: LDFLAGS+=-fPIC

# windows dynamic libtcc library
libtcc.dll : $(LIBTCC_OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
libtcc.dll : DEFINES += -DLIBTCC_AS_DLL

# import file for windows libtcc.dll
libtcc.def : libtcc.dll tcc$(EXESUF)
	$(XTCC) -impdef $< -o $@
XTCC ?= ./tcc$(EXESUF)

# TinyCC runtime libraries
libtcc1.a : tcc$(EXESUF) FORCE
	@$(MAKE) -C lib DEFINES='$(DEF-$T)'

# kiln.macos.zip:
# 	zip -r $@ build

# kiln.linux.zip:
# 	zip -r $@ build

# kiln.windows.zip:
# 	7za a -tzip $@ -r build

# build:
# 	mkdir build

FORCE:

# --------------------------------------------------------------------------
# documentation and man page
tcc-doc.html: tcc-doc.texi
	makeinfo --no-split --html --number-sections -o $@ $< || true

tcc.1: tcc-doc.texi
	$(TOPSRC)/texi2pod.pl $< tcc.pod \
	&& pod2man --section=1 --center="Tiny C Compiler" --release="$(VERSION)" tcc.pod >tmp.1 \
	&& mv tmp.1 $@ || rm -f tmp.1

tcc-doc.info: tcc-doc.texi
	makeinfo $< || true

# --------------------------------------------------------------------------
# install kiln

# bundle-kiln-unx:
# 	mkdir -p build/include
# 	mkdir -p build/lib
# 	mkdir -p build/src
# 	mkdir -p build/examples
# 	cp $(PROGS) build
# 	cp $(LIBTCC1) build
# 	cp $(TOPSRC)/src/tcclib.h $(TOPSRC)/src/libtcc.h build/include
# 	cp libtcc$(DLLSUF) build
# 	cp -r vendor/all/include/* build/include
# 	cp -r vendor/$(OSFLAG)/include/* build/include
# 	cp -r vendor/$(OSFLAG)/lib/* build/lib

# bundle-kiln-win:
# 	mkdir -p build/include
# 	mkdir -p build/lib
# 	mkdir -p build/src
# 	mkdir -p build/examples
# 	cp $(PROGS) build
# 	cp libtcc1.a build
# 	cp $(TOPSRC)/win32/lib/*.def build/lib
# 	cp $(TOPSRC)/tcclib.h $(TOPSRC)/libtcc.h build/include
# 	cp $(TOPSRC)/win32/include build/include
# 	cp $(TOPSRC)/win32/examples build/examples
# 	cp libtcc.dll libtcc.def build/lib

# 	cp -r vendor/all/include/* build/include
# 	cp -r vendor/$(OSFLAG)/include/* build/include
# 	cp -r vendor/$(OSFLAG)/lib/* build/lib


# --------------------------------------------------------------------------
# other stuff

TAGFILES = *.[ch] include/*.h lib/*.[chS]
tags : ; ctags $(TAGFILES)
# cannot have both tags and TAGS on windows
ETAGS : ; etags $(TAGFILES)

config.mak:
	$(if $(wildcard $@),,@echo "Please run ./configure." && exit 1)

# run all tests
test:
	$(MAKE) -C tests
# run test(s) from tests2 subdir (see make help)
tests2.%:
	$(MAKE) -C tests/tests2 $@

clean:
	rm -f kiln$(EXESUF) tcc$(EXESUF) tcc_p$(EXESUF) *-tcc$(EXESUF) tcc.pod
	rm -f  *~ *.o *.a *.so* *.out *.log lib*.def *.exe *.dll *.dylib a.out tags TAGS
	@$(MAKE) -C lib $@
	@$(MAKE) -C tests $@

distclean: clean
	rm -f config.h config.mak config.texi tcc.1 tcc-doc.info tcc-doc.html

.PHONY: all clean test tar tags ETAGS distclean install uninstall FORCE

help:
	@echo "make"
	@echo "   build native compiler (from separate objects)"
	@echo ""
	@echo "make cross"
	@echo "   build cross compilers (from one source)"
	@echo ""
	@echo "make ONE_SOURCE=yes / no"
	@echo "   force building from one source / separate objects"
	@echo ""
	@echo "make cross-TARGET"
	@echo "   build one specific cross compiler for 'TARGET', as in"
	@echo "   $(TCC_X)"
	@echo ""
	@echo "Custom configuration:"
	@echo "   The makefile includes a file 'config-extra.mak' if it is present."
	@echo "   This file may contain some custom configuration.  For example:"
	@echo ""
	@echo "      NATIVE_DEFINES += -D..."
	@echo ""
	@echo "   Or for example to configure the search paths for a cross-compiler"
	@echo "   that expects the linux files in <tccdir>/i386-linux:"
	@echo ""
	@echo "      ROOT-i386 = {B}/i386-linux"
	@echo "      CRT-i386  = {B}/i386-linux/usr/lib"
	@echo "      LIB-i386  = {B}/i386-linux/lib:{B}/i386-linux/usr/lib"
	@echo "      INC-i386  = {B}/lib/include:{B}/i386-linux/usr/include"
	@echo "      DEF-i386  += -D__linux__"
	@echo ""
	@echo "make test"
	@echo "   run all tests"
	@echo ""
	@echo "make tests2.all / make tests2.37 / make tests2.37+"
	@echo "   run all/single test(s) from tests2, optionally update .expect"
	@echo ""
	@echo "Other supported make targets:"
	@echo "   install install-strip tags ETAGS tar clean distclean help"
	@echo ""

# --------------------------------------------------------------------------
endif # ($(INCLUDED),no)
