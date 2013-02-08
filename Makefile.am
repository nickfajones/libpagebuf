ACLOCAL_AMFLAGS=-I m4

SUBDIRS=$(GENERIC_LIBRARY_NAME) test

DIST_SUBDIRS=$(GENERIC_LIBRARY_NAME) test
EXTRA_DIST=LICENSE NOTICE

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=pagebuf.pc

tests: all
	@(cd test && $(MAKE) tests)
