#ifndef __METALOG_H__
#define __METALOG_H__ 1

#ifndef __GNUC__
# ifdef __attribute__
#  undef __attribute__
# endif
# define __attribute__(a)
#endif

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
# include <stdarg.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#elif defined(HAVE_SYS_FCNTL_H)
# include <sys/fcntl.h>
#endif
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <dirent.h>
#include <syslog.h>
#ifndef HAVE_SYSLOG_NAMES
# include "syslognames.h"
#endif
#ifndef HAVE_GETOPT_LONG
# include "bsd-getopt_long.h"
#else
# include <getopt.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(st) ((unsigned) (st) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(st) (((st) & 0xff) == 0)
#endif
#ifdef HAVE_SYS_KLOG_H
# include <sys/klog.h>
#endif
#include <pcre.h>

#ifdef HAVE_ALLOCA
# ifdef HAVE_ALLOCA_H
#  include <alloca.h>
# endif
# define ALLOCA(X) alloca(X)
# define ALLOCA_FREE(X) do { } while (0)
#else
# define ALLOCA(X) malloc(X)
# define ALLOCA_FREE(X) free(X)
#endif

#include "mysnprintf.h"

#ifndef MAXPATHLEN
# ifdef PATH_MAX
#  define MAXPATHLEN PATH_MAX
# else
#  define MAXPATHLEN 65536U
Warning: neither PATH_MAX nor MAXPAHLEN were found.
Remove these lines if you really want to compile the server, but
the server may be insecure if a wrong value is set here.
# endif
#endif
#if (MAXPATHLEN) >= (INT_MAX)
Your platform has a very large maximum path len, we should not trust it.
#endif

#ifdef _PATH_LOG
# define SOCKNAME _PATH_LOG
#else
# define SOCKNAME "/dev/log"
#endif
#ifdef _PATH_KLOG
# define KLOG_FILE _PATH_KLOG
#else
# define KLOG_FILE "/dev/klog"
#endif

#ifndef errno
extern int errno;
#endif

typedef struct PCREInfo_ {
    pcre *pcre;
    pcre_extra *pcre_extra;
} PCREInfo;

typedef struct DuplicateTracker_ {
    char *previous_prg;
    char *previous_info;
    size_t sizeof_previous_prg;
    size_t sizeof_previous_info;
    size_t previous_sizeof_prg;
    size_t previous_sizeof_info;
    unsigned int same_counter;
} DuplicateTracker;

typedef struct Output_ {
    char *directory;
    FILE *fp;
    off_t size;
    off_t maxsize;
    int maxfiles;
    time_t maxtime;
    time_t creatime;
    DuplicateTracker dt;
    struct Output_ *next_output;
    char *postrotate_cmd;
} Output;

typedef enum RegexSign_ {
    REGEX_SIGN_POSITIVE, REGEX_SIGN_NEGATIVE
} RegexSign;

typedef struct RegexWithSign_ {
    PCREInfo regex;
    RegexSign sign;
} RegexWithSign;

typedef struct ConfigBlock_ {
    int minimum;
    int maximum;
    int *facilities;
    int nb_facilities;
    RegexWithSign *regexeswithsign;
    int nb_regexes;
    off_t maxsize;
    int maxfiles;
    time_t maxtime;
    Output *output;
    const char *command;
    const char *program;
    /*
     * If output or command match, and brk is set,
     * do output and/or command then break out of
     * match loop, i.e. don't consider any more
     * sections below this one in the config file.
     */
    int brk;
    RegexWithSign *program_regexeswithsign;
    int program_nb_regexes;
    struct ConfigBlock_ *next_block;
    char *postrotate_cmd;
} ConfigBlock;

typedef enum LogLineType_ {
        LOGLINETYPE_SYSLOG,
        LOGLINETYPE_KLOG
} LogLineType;

#define DEFAULT_MINIMUM LOG_DEBUG
#define DEFAULT_MAXIMUM INT_MAX
#define DEFAULT_MAXSIZE ((off_t) 1000000)
#define DEFAULT_MAXFILES 5
#define DEFAULT_MAXTIME ((time_t) 60 * 60 * 24)
#define DEFAULT_CONSOLE_LEVEL 7
#define MIN_CONSOLE_LEVEL 0
#define MAX_CONSOLE_LEVEL 7
#define CF_PROGNAME_KERNEL "kernel"
#define OUTPUT_DIR_TIMESTAMP ".timestamp"
#define OUTPUT_DIR_CURRENT "current"
#define OUTPUT_DIR_PERMS 0700
#define OUTPUT_DIR_LOGFILES_PREFIX "log-"   /* log-YYYY-MM-DD-HH:MM:SS */
#define DEFAULT_CONFIG_FILE CONFDIR "/metalog.conf"
#define DEFAULT_PID_FILE "/var/run/metalog.pid"
#define NONPRINTABLE_SUSTITUTE_CHAR '_'
#define PROGNAME_MASTER "metalog [MASTER]"
#define PROGNAME_KERNEL "metalog [KERNEL]"
#define LAST_OUTPUT "                - Last output repeated %u times -\n"
#define LAST_OUTPUT_TWICE "                - Last output repeated twice -\n"
#define MAX_SIGNIFICANT_LENGTH 512U
#define MAX_LOG_LENGTH 8192U          /* must be < (INT_MAX / 2) */

#ifndef HAVE_STRTOULL
# ifdef HAVE_STRTOQ
#  define strtoull(X, Y, Z) strtoq(X, Y, Z)
# else
#  define strtoull(X, Y, Z) strtoul(X, Y, Z)
# endif
#endif

#ifdef ACCEPT_UNICODE_CONTROL_CHARS
# define ISCTRLCODE(X) ((X) == 0x7f || ((unsigned char) (X)) < 32U)
#else
# define ISCTRLCODE(X) ((X) == 0x7f || !(((unsigned char) (X)) & 0x60))
#endif

#endif
