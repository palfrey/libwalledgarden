#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT(libwalledgarden, 0.1, walledgarden@tevp.net)
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES(PCRE,libpcre)
PKG_CHECK_MODULES(CURL,libcurl)
PKG_CHECK_MODULES(RAPTOR,raptor)
PKG_CHECK_MODULES(GLIB,glib-2.0)
# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdlib.h string.h unistd.h ltdl.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([memchr memset mkdir regcomp strdup strncasecmp strtol])

AC_OUTPUT(
	Makefile
	src/Makefile
	src/modules/Makefile
	walledgarden.pc
)
