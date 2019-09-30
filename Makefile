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

PROGS = tinycc/tcc$(EXESUF) tinycc/clay$(EXESUF)
TCCLIBS = tinycc/libtcc1.a tinycc/libtcc$(DLLSUF) $(LIBTCCDEF)

all: $(PROGS) $(TCCLIBS)

zip: clay.$(OSFLAG).zip

bundle: tinycc/clay
	@$(MAKE) --no-print-directory bundle-clay$(CFGWIN)

# --------------------------------------------

clay.macos.zip: bundle
	zip -r $@ clay

clay.linux.zip: bundle
	zip -r $@ clay

clay.windows.zip: bundle
	7za a -tzip $@ -r clay

tinycc/clay: tinycc/config.mak
	 $(MAKE) -C tinycc

tinycc/tcc: tinycc/config.mak
	 $(MAKE) -C tinycc

tinycc/config.mak:
	cd tinycc && ./configure --disable-static

FORCE:


# --------------------------------------------------------------------------
# install clay

bundle-clay-unx:
	mkdir -p clay/include
	mkdir -p clay/lib
	mkdir -p clay/src
	mkdir -p clay/examples
	cp $(PROGS) clay
	cp $(TCCLIBS) clay
	cp tinycc/src/tcclib.h tinycc/src/libtcc.h clay/include
	cp -r vendor/all/include/* clay/include
	cp -r vendor/$(OSFLAG)/include/* clay/include
	cp -r vendor/$(OSFLAG)/lib/* clay/lib
	cp -r examples/* clay/src

bundle-clay-win:
	mkdir -p clay/include
	mkdir -p clay/lib
	mkdir -p clay/src
	mkdir -p clay/examples
	cp $(PROGS) clay
	cp tinycc/libtcc1.a clay
	cp tinycc/win32/lib/*.def clay/lib
	cp tinycc/tcclib.h tinycc/libtcc.h clay/include
	cp tinycc/win32/include clay/include
	cp tinycc/win32/examples clay/examples
	cp tinycc/libtcc.dll tinycc/libtcc.def clay/lib

	cp -r vendor/all/include/* clay/include
	cp -r vendor/$(OSFLAG)/include/* clay/include
	cp -r vendor/$(OSFLAG)/lib/* clay/lib
	cp -r examples/* clay/src

# --------------------------------------------------------------------------
# other stuff

clean:
	@$(MAKE) -C tinycc $@

distclean: clean
	rm -r clay
	rm -f clay.$(OSFLAG).zip

.PHONY: all bundle zip clean distclean FORCE
#
# --------------------------------------------------------------------------
