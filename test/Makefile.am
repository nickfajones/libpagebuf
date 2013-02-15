LDADD = $(top_builddir)/pagebuf/.libs/libpagebuf.a -lssl
AM_CFLAGS = -Wall -Werror -Wextra -pedantic -g3 -std=gnu99
AM_CPPFLAGS = -I$(top_srcdir) -Wno-write-strings
check_PROGRAMS = test_rnd1

test_rnd1_SOURCES = test_rnd1.c

tests: $(check_PROGRAMS)
