# --------------------------------------------------------------------------
# Detect platform
#
ifeq ($(OS),Windows_NT)
	OSFLAG = windows
	DLLSUF = .dll
	EXESUF = .exe
	CFGWIN = -win
	LIBTCCDEF = tinycc/libtcc.def
else
	CFGWIN = -unx
	EXESUF =
	uname_s := $(shell uname -s)
	ifeq ($(uname_s),Linux)
		OSFLAG = linux
		DLLSUF = .so
	else ifeq ($(uname_s),Darwin)
		OSFLAG = macos
		DLLSUF = .dylib
	else
		OSFLAG = unsupported
	endif
endif


# --------------------------------------------------------------------------
# running top Makefile

PROGS = tinycc/kiln$(EXESUF) # tinycc/tcc$(EXESUF)
TCCLIBS = tinycc/libtcc1.a tinycc/libtcc$(DLLSUF) $(LIBTCCDEF)

all: $(PROGS) $(TCCLIBS)

zip: kiln.$(OSFLAG).zip

bundle: tinycc/kiln
	@$(MAKE) --no-print-directory bundle-kiln$(CFGWIN)

# --------------------------------------------

kiln.macos.zip: bundle
	zip -r $@ kiln

kiln.linux.zip: bundle
	zip -r $@ kiln

kiln.windows.zip: bundle
	7za a -tzip $@ -r kiln

tinycc/kiln: tinycc/config.mak
	 $(MAKE) -C tinycc

tinycc/tcc: tinycc/config.mak
	 $(MAKE) -C tinycc

tinycc/config.mak:
	cd tinycc && ./configure --disable-static

$(PROGS): FORCE

FORCE:


# --------------------------------------------------------------------------
# install kiln

bundle-kiln-unx:
	mkdir -p kiln/include
	mkdir -p kiln/lib
	mkdir -p kiln/src
	mkdir -p kiln/plugin
	mkdir -p kiln/examples
	mkdir -p kiln/etc
	cp $(PROGS) kiln
	cp $(TCCLIBS) kiln
	cp tinycc/src/tcclib.h tinycc/src/libtcc.h kiln/include
	cp tinycc/src/clay.h kiln/include

	cp -r vendor/all/include/* kiln/include
	cp -r vendor/$(OSFLAG)/include/* kiln/include
	cp -r vendor/$(OSFLAG)/lib/* kiln/lib

	cp -r examples/* kiln/examples
	cp -r etc/* kiln/etc

# 	touch kiln/.kiln

bundle-kiln-win:
	mkdir -p kiln/include
	mkdir -p kiln/lib
	mkdir -p kiln/src
	mkdir -p kiln/plugin
	mkdir -p kiln/examples
	mkdir -p kiln/etc
	cp $(PROGS) kiln
	cp tinycc/libtcc1.a kiln
	cp tinycc/win32/lib/*.def kiln/lib
	cp tinycc/src/tcclib.h tinycc/src/libtcc.h kiln/include
	cp tinycc/src/clay.h kiln/include

	cp tinycc/win32/include kiln/include
	cp tinycc/win32/examples kiln/examples/win32
	cp tinycc/libtcc.dll tinycc/libtcc.def kiln/lib

	cp -r vendor/all/include/* kiln/include
	cp -r vendor/$(OSFLAG)/include/* kiln/include
	cp -r vendor/$(OSFLAG)/lib/* kiln/lib

	cp -r examples/* kiln/examples
	cp -r etc/* kiln/etc

# 	touch kiln/.kiln

# --------------------------------------------------------------------------
# other stuff

clean:
	@$(MAKE) -C tinycc $@

distclean: clean
	rm -r kiln
	rm -f kiln.$(OSFLAG).zip

.PHONY: all bundle zip clean distclean FORCE
#
# --------------------------------------------------------------------------
