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

typedef struct Output_ {
    char *directory;
    FILE *fp;
    off_t size;
    off_t maxsize;
    int maxfiles;
    time_t maxtime;
    time_t creatime;
    struct Output_ *next_output;
} Output;

#define FAC_STATE_NOTSET 0
#define FAC_STATE_ALL 1
#define FAC_STATE_ADD 2
#define FAC_STATE_NEG 3
#define BN_LENGHT 20 /* The max length of a blockname */

typedef struct ConfigBlock_ {
    int minimum;
    int *facilities;     
    char facility_state; /* ie, are we negating, or or we adding */
    int nb_facilities;
    PCREInfo *regexes;
    char *regex_state;
    int nb_regexes;
    off_t maxsize;
    int maxfiles;
    time_t maxtime;
    Output *output;
    const char *command;
    const char *program;
    PCREInfo *prog_regexes;
    char *prog_regex_state;
    int nb_prog_regexes;
    struct ConfigBlock_ *next_block;
} ConfigBlock;

typedef enum LogLineType_ {
        LOGLINETYPE_SYSLOG,
        LOGLINETYPE_KLOG
} LogLineType;

#define DEFAULT_MINIMUM LOG_DEBUG
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
#define CONFIG_FILE "/etc/metalog.conf"
#define NONPRINTABLE_SUSTITUTE_CHAR ' '
#define PROGNAME_MASTER "metalog [MASTER]"
#define PROGNAME_KERNEL "metalog [KERNEL]"

#ifndef HAVE_STRTOULL
# ifdef HAVE_STRTOQ
#  define strtoull(X, Y, Z) strtoq(X, Y, Z)
# else
#  define strtoull(X, Y, Z) strtoul(X, Y, Z)
# endif
#endif

#endif
