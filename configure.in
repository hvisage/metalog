AC_PREREQ(2.50)
AC_INIT
AC_CONFIG_SRCDIR(src/metalog.c)
AM_INIT_AUTOMAKE(metalog, 0.6)
AM_CONFIG_HEADER(config.h)

AC_SUBST(VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PROG_CC
AC_AIX
AC_ISC_POSIX
AC_PROG_CC_STDC
CFLAGS="$CFLAGS -D_GNU_SOURCE=1 -I/usr/local/include "
LDFLAGS="$LDFLAGS -L/usr/local/lib "

AC_CANONICAL_HOST

dnl Checks for header files

AC_HEADER_STDC
AC_HEADER_STAT
AC_HEADER_TIME
AC_HEADER_DIRENT
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(sys/time.h sys/wait.h fcntl.h sys/fcntl.h)
AC_CHECK_HEADERS(getopt.h stddef.h sys/klog.h)

dnl Checks for types

AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_MODE_T

AC_CHECK_TYPE(nlink_t, , [AC_DEFINE(nlink_t, int)],
[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
])

AC_CHECK_TYPE(socklen_t, , [AC_DEFINE(socklen_t, int)],
[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
])

AC_CHECK_TYPE(sig_atomic_t, , [AC_DEFINE(sig_atomic_t, signed char)],
[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <signal.h>
])

dnl Compiler characteristics

AC_PROG_GCC_TRADITIONAL
AC_C_CONST
AC_C_INLINE

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
  AC_DEFINE(HAVE_SYSLOG_NAMES)
],
[
  AC_MSG_RESULT(no)
])  

dnl Checks for libraries.

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_STRFTIME
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_FUNC_GETGROUPS

AC_CHECK_FUNCS(initgroups setresuid setrlimit waitpid setproctitle getopt_long)
AC_CHECK_FUNCS(putenv strtoull strtoq klogctl)

dnl Socket things

AC_CHECK_FUNC(connect, , [AC_CHECK_LIB(socket, connect)])

dnl PCRE

AC_CHECK_LIB(pcre, pcre_compile, ,
[AC_ERROR([You need to install the PCRE library - http://www.pcre.org])])

dnl Arguments

AC_ARG_WITH(debug,
[AC_HELP_STRING([--with-debug],[For maintainers only - please do not use])],
[ if test "x$withval" = "xyes" ; then
    CFLAGS="$CFLAGS -DDEBUG=1 -g -Wall -W -Wcast-qual -Wcast-align -Wmissing-noreturn -Wbad-function-cast -Wstrict-prototypes -Wwrite-strings -Wreturn-type "
  fi ])

AC_CONFIG_FILES(Makefile src/Makefile man/Makefile metalog.spec metalog.lsm)

AC_OUTPUT


