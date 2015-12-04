ACLOCAL_AMFLAGS = -I m4

SUBDIRS = pagebuf test

DIST_SUBDIRS = pagebuf test
EXTRA_DIST = LICENSE NOTICE

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pagebuf.pc

test: all
	@(cd test && $(MAKE) $@)
