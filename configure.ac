AC_PREREQ(2.68)
AC_INIT([metalog], [4], [metalog-users@lists.sourceforge.net])
AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip silent-rules -Wall])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR(src/metalog.c)
AC_CONFIG_HEADERS([config.h])

gl_cv_func_printf_infinite_long_double=yes

dnl Checks for programs.
AC_PROG_INSTALL
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_SYS_LARGEFILE
PKG_PROG_PKG_CONFIG

dnl Checks for header files

AC_CHECK_HEADERS(sys/klog.h)

dnl Checks for typedefs, structures, and compiler characteristics.

AC_PROG_GCC_TRADITIONAL
AC_C_INLINE
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_STRUCT_TM
AC_TYPE_UID_T
AC_TYPE_OFF_T

dnl Options

AM_WITH_DMALLOC

AC_MSG_CHECKING([whether syslog names are available])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[
			#define SYSLOG_NAMES 1
			#include <stdio.h>
			#include <syslog.h>
		]],
		[[
			(void) facilitynames
		]],
	)],
	[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_SYSLOG_NAMES, , [Define if syslog names are defined])
	],
	[
		AC_MSG_RESULT(no)
	])

dnl Checks for libraries.

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_FUNC_STRFTIME
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_FUNC_GETGROUPS

AC_CHECK_FUNCS_ONCE(m4_flatten([
	daemon
	klogctl
	setproctitle
	setprogname
]))

dnl PCRE2

PKG_CHECK_MODULES([PCRE2], [libpcre2-8])
PKG_CHECK_MODULES([ZLIB], [zlib])
CFLAGS="$CFLAGS $PCRE2_CFLAGS $ZLIB_CFLAGS"
LIBS="$LIBS $PCRE2_LIBS $ZLIB_LIBS"

dnl Options

AM_WITH_DMALLOC

WFLAGS=""
AC_DEFUN([W_CHECK_CFLAG],[AX_CHECK_COMPILE_FLAG([$1],[WFLAGS="$WFLAGS $1"])])
W_CHECK_CFLAG([-Wall])
W_CHECK_CFLAG([-W])
W_CHECK_CFLAG([-Wcast-qual])
W_CHECK_CFLAG([-Wcast-align])
W_CHECK_CFLAG([-Wmissing-noreturn])
W_CHECK_CFLAG([-Wbad-function-cast])
W_CHECK_CFLAG([-Wstrict-prototypes])
W_CHECK_CFLAG([-Wwrite-strings])
W_CHECK_CFLAG([-Wreturn-type])
AC_SUBST(WFLAGS, $WFLAGS)

AC_ARG_WITH(unicode,
	[AS_HELP_STRING([--with-unicode],[Enable unicode support (default no)])])
AS_IF([test "x$with_unicode" = "xyes"], [
	AC_DEFINE([ACCEPT_UNICODE_CONTROL_CHARS], [1], [Don't scramble unicode])
])

AC_CONFIG_FILES(m4_flatten([
	Makefile
	src/Makefile
	man/Makefile
	man/metalog.8
	man/metalog.conf.5
	metalog.spec
	metalog.lsm
]))

AC_OUTPUT
