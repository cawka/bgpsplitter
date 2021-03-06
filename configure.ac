#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT([bgpsplitter], [0.1], [alexander.afanasyev@ucla.edu])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for libraries.
AX_LOG4CXX

PKG_CHECK_MODULES( bgpparser, [bgpparser >= 0.2.1] )
AC_SUBST(bgpparser_CFLAGS)
AC_SUBST(bgpparser_LIBS)

AX_BOOST_BASE( [1.44],, [AC_MSG_ERROR([Boost C++ Libraries are not installed, but are required by libbgpparser])] )
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM
AX_BOOST_PROGRAM_OPTIONS
AX_BOOST_REGEX
AX_BOOST_IOSTREAMS

# AC_PATH_PROGS(PERL, perl)

AX_DEBUG

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile
                 src/Makefile
				 scripts/Makefile
				 ])
AC_OUTPUT
