
dnl ----------------------
dnl Initialization macros
dnl ----------------------

AC_INIT([libpagebuf], [0.9])
AM_INIT_AUTOMAKE()

AC_CONFIG_HEADERS([config.h])

dnl -----------------------------------------------
dnl Package name and version number (user defined)
dnl -----------------------------------------------

GENERIC_LIBRARY_NAME=pagebuf

GENERIC_MAJOR_VERSION=0
GENERIC_MINOR_VERSION=9
GENERIC_MICRO_VERSION=0

# API version (often = GENERIC_MAJOR_VERSION.GENERIC_MINOR_VERSION)
GENERIC_API_VERSION=$GENERIC_MAJOR_VERSION.$GENERIC_MINOR_VERSION
AC_SUBST(GENERIC_API_VERSION)

# Shared library versioning
GENERIC_LIBRARY_VERSION=$GENERIC_MAJOR_VERSION:$GENERIC_MINOR_VERSION:$GENERIC_MICRO_VERSION
AC_SUBST(GENERIC_LIBRARY_VERSION)

dnl --------------------------------
dnl Package name and version number
dnl --------------------------------

PACKAGE=$GENERIC_LIBRARY_NAME
AC_SUBST(GENERIC_LIBRARY_NAME)

GENERIC_VERSION=$GENERIC_MAJOR_VERSION.$GENERIC_MINOR_VERSION.$GENERIC_MICRO_VERSION
GENERIC_RELEASE=$GENERIC_MAJOR_VERSION.$GENERIC_MINOR_VERSION
AC_SUBST(GENERIC_VERSION)
AC_SUBST(GENERIC_RELEASE)

VERSION=$GENERIC_VERSION

AC_CONFIG_MACRO_DIR([m4])

dnl -----------------------------------------------
dnl Checks for programs.
dnl -----------------------------------------------

AC_PROG_CC
AC_PROG_CXX
AM_PROG_LIBTOOL
AM_SANITY_CHECK

CFLAGS=""
CXXFLAGS=""

# Determine the OS 
AC_MSG_CHECKING([OS])
OS=`uname -s`
case "$OS" in
        CYGWIN*)
                AC_MSG_RESULT(Cygwin)
                OS_CYGWIN="true"
                NO_STACK_PROTECTOR="true"
        ;;
        FreeBSD*)
                AC_MSG_RESULT(FreeBSD)
                OS_FREEBSD="true"
                NO_STACK_PROTECTOR="true"
                CPPFLAGS="${CPPFLAGS} -I/usr/local/include"
                LDFLAGS="${LDFLAGS} -L/usr/local/lib"
        ;;
        Linux*)
                AC_MSG_RESULT(Linux)
                OS_LINUX="true"
        ;;
        *)
                AC_MSG_RESULT(no)
        ;;
esac

dnl -----------------------------------------------
dnl Check and enable the GCC opts we want to use.
dnl We may need to add more checks
dnl -----------------------------------------------

dnl -----------------------------------------------
dnl Check for GCC signed overflow warning support
dnl -----------------------------------------------
AC_MSG_CHECKING(for gcc support of -Wstrict-overflow=1)
TMPCFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Wstrict-overflow=1"
AC_TRY_COMPILE(,,[gcc_have_strict_overflow=yes],[gcc_have_strict_overflow=no])
AC_MSG_RESULT($gcc_have_strict_overflow)
if test "$gcc_have_strict_overflow" != "yes"; then
    CFLAGS="${TMPCFLAGS}"
fi

if test "$NO_STACK_PROTECTOR" != "true"; then
dnl -----------------------------------------------
dnl Check for GCC stack smashing protection
dnl -----------------------------------------------
AC_MSG_CHECKING(for gcc support of stack smashing protection)
TMPCFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -fstack-protector"
AC_TRY_COMPILE(,,[gcc_have_fstack_protector=yes],[gcc_have_fstack_protector=no])
AC_MSG_RESULT($gcc_have_fstack_protector)
if test "$gcc_have_fstack_protector" != "yes"; then
    CFLAGS="${TMPCFLAGS}"
fi
fi

dnl -----------------------------------------------
dnl Check for GCC -D_FORTIFY_SOURCE support
dnl -----------------------------------------------
AC_MSG_CHECKING(for gcc support of FORTIFY_SOURCE)
TMPCFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -D_FORTIFY_SOURCE=2"
AC_TRY_COMPILE(,,[gcc_have_fortify_source=yes],[gcc_have_fortify_source=no])
AC_MSG_RESULT($gcc_have_fortify_source)
if test "$gcc_have_fortify_source" != "yes"; then
    CFLAGS="${TMPCFLAGS}"
fi

dnl -----------------------------------------------
dnl Check for GCC -Wformat-security support
dnl -----------------------------------------------
AC_MSG_CHECKING(for gcc support of -Wformat -Wformat-security)
TMPCFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Wformat -Wformat-security"
AC_TRY_COMPILE(,,[gcc_have_format_security=yes],[gcc_have_format_security=no])
AC_MSG_RESULT($gcc_have_format_security)
if test "$gcc_have_format_security" != "yes"; then
    CFLAGS="${TMPCFLAGS}"
fi

dnl -----------------------------------------------
dnl Construct final compiler flag set
dnl -----------------------------------------------
AC_ARG_ENABLE(debug,   [  --enable-debug    Enable debug mode], [ enable_debug=yes ])
if test "$enable_debug" = "yes"; then
    OPTFLAGS="-O0 -g3"
    COMMON_CFLAGS="-Wall -Werror -Wfatal-errors"
    CONFIG_CFLAGS="${CFLAGS}"
    CFLAGS="${COMMON_CFLAGS} ${OPTFLAGS} -std=gnu99 ${CONFIG_CFLAGS}"
    CXXFLAGS="${COMMON_CFLAGS} ${OPTFLAGS} -std=gnu++0x ${CONFIG_CFLAGS} ${CXXFLAGS}"
    CPPFLAGS="-D_GNU_SOURCE ${CPPFLAGS}"
    echo "Debug mode enabled"
else
    OPTFLAGS="-O2 -g"
    COMMON_CFLAGS="-Wall"
    CONFIG_CFLAGS="${CFLAGS}"
    CFLAGS="${COMMON_CFLAGS} ${OPTFLAGS} -std=gnu99 ${CONFIG_CFLAGS}"
    CXXFLAGS="${COMMON_CFLAGS} ${OPTFLAGS} -std=gnu++0x ${CONFIG_CFLAGS} ${CXXFLAGS}"
    CPPFLAGS="-D_GNU_SOURCE -DNDEBUG ${CPPFLAGS}"
fi

dnl -----------------------------------------------
dnl Check for doxygen
dnl -----------------------------------------------
AC_ARG_WITH([doxygen],
            [  --with-doxygen=PROG doxygen executable],
            [doxygen="$withval"],[doxygen=no])

if test "$doxygen" != "no"; then
    AC_MSG_NOTICE([Using doxygen: $doxygen])
else
    AC_PATH_PROGS([doxygen],[doxygen],[])
fi

DOXYGEN=$doxygen
AC_SUBST(DOXYGEN)

dnl -----------------------------------------------
dnl Generates Makefiles, configuration files and scripts
dnl -----------------------------------------------

AC_PREFIX_DEFAULT(/usr/local)
AC_OUTPUT(Makefile \
          pagebuf.pc \
          pagebuf/Makefile \
          test/Makefile
)

