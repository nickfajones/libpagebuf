LDADD = $(top_builddir)/pagebuf/libpagebuf.la -lcrypto

AM_CFLAGS = -D_GNU_SOURCE -O0 -g3 -Wall -Wextra -std=gnu99 -pedantic \
  -Wextra -Wno-missing-field-initializers -Wshadow -Wpointer-arith \
  -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_builddir)/pagebuf -Wno-write-strings \
  -D_GNU_SOURCE -O0 -g3 -Wall -Wextra -Wno-unused-parameter

AUTOMAKE_OPTIONS = subdir-objects
EXTRA_DIST = files
check_PROGRAMS = test_ops test_rnd1 test_rnd2

test_ops_SOURCES = test_ops.cpp
test_rnd1_SOURCES = test_rnd1.cpp
test_rnd2_SOURCES = test_rnd2.cpp

TESTS = test_ops test_rnd1 test_rnd2

test: check
	@echo

test-compile-only: all-am
	$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS)

check-compile-only: all-am
	$(MAKE) $(AM_MAKEFLAGS) ${check_PROGRAMS)

