OPTFLAGS = -g3 -O0
AM_CPPFLAGS = -I$(top_srcdir)
LDADD = $(top_builddir)/pagebuf/.libs/libpagebuf.a -lcrypto
check_PROGRAMS = test_rnd1

test_rnd1_SOURCES = test_rnd1.cpp

tests: $(check_PROGRAMS)
