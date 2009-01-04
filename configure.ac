AC_PREREQ(2.61)
AC_INIT([metalog], [0.9], [metalog-users@lists.sourceforge.net])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR(src/metalog.c)
AM_CONFIG_HEADER(config.h)

AC_SUBST(VERSION)

dnl Lie about crap we dont care about

gl_cv_func_printf_infinite_long_double=yes

dnl Checks for programs.
AC_PROG_INSTALL
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
gl_EARLY
gl_INIT
AC_PROG_CC_STDC
AC_SYS_LARGEFILE

dnl Use -Wall if we have gcc.
if test "x$GCC" = "xyes"; then
	CFLAGS="$CFLAGS -Wall"
fi

dnl Checks for header files

AC_CHECK_HEADERS(sys/klog.h)

dnl Checks for typedefs, structures, and compiler characteristics.

AC_PROG_GCC_TRADITIONAL
AC_C_INLINE
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_STRUCT_TM
AC_TYPE_SIGNAL
AC_TYPE_UID_T
AC_TYPE_OFF_T

dnl Options

AM_WITH_DMALLOC

AC_MSG_CHECKING([whether syslog names are available])
AC_TRY_COMPILE([
#define SYSLOG_NAMES 1
#include <stdio.h>
#include <syslog.h>
],
[
 (void) facilitynames
],
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

AC_CHECK_FUNCS(setproctitle setprogname)
AC_CHECK_FUNCS(klogctl)

dnl PCRE

AC_CHECK_LIB(pcre, pcre_compile, ,
[AC_ERROR([You need to install the PCRE library - http://www.pcre.org])])

dnl Options

AM_WITH_DMALLOC

AC_ARG_WITH(debug,
[AC_HELP_STRING([--with-debug],[For maintainers only - please do not use])],
[ if test "x$withval" = "xyes" ; then
    CFLAGS="$CFLAGS -DDEBUG=1 -g -Wall -W -Wcast-qual -Wcast-align -Wmissing-noreturn -Wbad-function-cast -Wstrict-prototypes -Wwrite-strings -Wreturn-type "
  fi ])

AC_CONFIG_FILES([ \
	Makefile \
	lib/Makefile \
	src/Makefile \
	man/Makefile \
	man/metalog.8 \
	man/metalog.conf.5 \
	metalog.spec \
	metalog.lsm \
])

AC_OUTPUT
