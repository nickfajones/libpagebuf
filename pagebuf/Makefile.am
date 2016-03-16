h_sources = pagebuf.h pagebuf_mmap.h pagebuf.hpp pagebuf_mmap.hpp

h_sources_private = pagebuf_hash.h

c_sources = pagebuf.c pagebuf_mmap.c

library_includedir = $(includedir)/$(GENERIC_LIBRARY_NAME)
library_include_HEADERS = $(h_sources)

AM_CPPFLAGS = -I$(top_srcdir) -D_GNU_SOURCE
AM_CFLAGS = -Wall -Wextra -pedantic \
  -Wstrict-overflow -fno-strict-aliasing \
  -Wno-missing-field-initializers -Wshadow -Wpointer-arith \
  -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter \
  -std=c99

noinst_LTLIBRARIES = libpagebuf-c.la
libpagebuf_c_la_SOURCES = $(h_sources) $(h_sources_private) $(c_sources)

lib_LTLIBRARIES = libpagebuf.la
libpagebuf_la_SOURCES =
libpagebuf_la_LIBADD = libpagebuf-c.la
libpagebuf_la_LDFLAGS = -version-info $(GENERIC_LIBRARY_VERSION) -release $(GENERIC_RELEASE)

