LDADD = $(top_builddir)/pagebuf/libpagebuf.la -lcrypto

AM_LDFLAGS = -no-install

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_builddir)/pagebuf -D_GNU_SOURCE
AM_CXXFLAGS = -Wall -Wextra \
  -Wstrict-overflow -fno-strict-aliasing \
  -Wno-missing-field-initializers -Wshadow -Wpointer-arith \
  -Wno-unused-parameter \
  -std=c++0x

AUTOMAKE_OPTIONS = subdir-objects
EXTRA_DIST = files
check_PROGRAMS = test_ops test_rnd1 test_rnd2 test_rnd3

test_ops_SOURCES = test_ops.cpp
test_rnd1_SOURCES = test_rnd1.cpp
test_rnd2_SOURCES = test_rnd2.cpp
test_rnd3_SOURCES = test_rnd3.cpp

TESTS = test_ops test_rnd1 test_rnd2 test_rnd3

test: check
	@echo

test-compile-only: all-am
	$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS)

check-compile-only: all-am
	$(MAKE) $(AM_MAKEFLAGS) ${check_PROGRAMS)

