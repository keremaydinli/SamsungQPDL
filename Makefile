VERSION ?= 0.1.0

CC       = clang
ARCHS    = -arch arm64 -arch x86_64
CFLAGS   = -O2 -Wall -Wextra $(ARCHS) -mmacosx-version-min=11.0 \
           -Ithird_party/jbigkit
JBIG_SRC = third_party/jbigkit/jbig85.c third_party/jbigkit/jbig_ar.c
QPDL_SRC = src/qpdl.c $(JBIG_SRC)
QPDL_DEP = $(QPDL_SRC) src/qpdl.h third_party/jbigkit/jbig85.h

PREFIX   = /Library/Printers/SamsungQPDL
PPDDIR   = /Library/Printers/PPDs/Contents/Resources
PPD      = Samsung-ML-1675-QPDL.ppd
QUEUE   ?= Samsung_ML_1670_Series
PKG      = build/SamsungQPDL-$(VERSION).pkg

all: rastertoqpdl testpage

# libcups' raster API is marked deprecated on macOS but is still what
# CUPS filters use.
rastertoqpdl: src/rastertoqpdl.c $(QPDL_DEP)
	$(CC) $(CFLAGS) -Wno-deprecated-declarations -o $@ \
		src/rastertoqpdl.c $(QPDL_SRC) -lcups

testpage: src/testpage.c $(QPDL_DEP)
	$(CC) $(CFLAGS) -o $@ src/testpage.c $(QPDL_SRC)

# Copy the filter and PPD into place. Needs root: sudo make install
install: rastertoqpdl
	install -d -o root -g wheel -m 755 $(PREFIX)/Filters
	install -o root -g wheel -m 755 rastertoqpdl $(PREFIX)/Filters/rastertoqpdl
	gzip -9 -c ppd/$(PPD) > $(PPDDIR)/$(PPD).gz
	chown root:wheel $(PPDDIR)/$(PPD).gz
	chmod 644 $(PPDDIR)/$(PPD).gz

# Add (or update) a print queue for the first USB ML-1670/1675 found.
# Needs root: sudo make setup-queue
setup-queue:
	@uri=$$(lpinfo -v | awk '$$2 ~ /^usb:\/\/Samsung\/ML-167/ { print $$2; exit }'); \
	if [ -z "$$uri" ]; then \
		echo "No Samsung ML-1670/1675 found on USB."; exit 1; \
	fi; \
	lpadmin -p $(QUEUE) -E -v "$$uri" -P ppd/$(PPD) -D "Samsung ML-1670/1675" && \
	echo "Printer $(QUEUE) set up at $$uri"

uninstall:
	rm -rf $(PREFIX)
	rm -f $(PPDDIR)/$(PPD).gz
	-pkgutil --forget io.github.keremaydinli.samsungqpdl >/dev/null 2>&1

# Installer package for people who don't want to build from source.
pkg: rastertoqpdl
	rm -rf build/root
	install -d build/root$(PREFIX)/Filters build/root$(PPDDIR)
	install -m 755 rastertoqpdl build/root$(PREFIX)/Filters/rastertoqpdl
	gzip -9 -c ppd/$(PPD) > build/root$(PPDDIR)/$(PPD).gz
	chmod 644 build/root$(PPDDIR)/$(PPD).gz
	xattr -cr build/root
	COPYFILE_DISABLE=1 pkgbuild --root build/root --install-location / \
		--identifier io.github.keremaydinli.samsungqpdl \
		--version $(VERSION) --ownership recommended $(PKG)

clean:
	rm -rf rastertoqpdl testpage test.qpdl build

.PHONY: all install setup-queue uninstall pkg clean
