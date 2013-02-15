h_sources = pagebuf.h

c_sources = pagebuf.c

library_includedir = $(includedir)/$(GENERIC_LIBRARY_NAME)
library_include_HEADERS = $(h_sources)

INCLUDES = -I$(top_srcdir)
AM_CFLAGS = -D_GNU_SOURCE -Wall -Werror -Wextra -pedantic \
  -std=gnu99 -Wno-missing-field-initializers -Wshadow -Wpointer-arith \
  -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter

noinst_LTLIBRARIES = libpagebuf-c.la
libpagebuf_c_la_SOURCES = $(h_sources) $(c_sources)

lib_LTLIBRARIES = libpagebuf.la
libpagebuf_la_SOURCES =
libpagebuf_la_LIBADD = libpagebuf-c.la
libpagebuf_la_LDFLAGS = -version-info $(GENERIC_LIBRARY_VERSION) -release $(GENERIC_RELEASE)

