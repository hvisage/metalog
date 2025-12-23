#ifndef __METALOG_H__
#define __METALOG_H__ 1

#ifndef __GNUC__
# ifdef __attribute__
#  undef __attribute__
# endif
# define __attribute__(a)
#endif

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <grp.h>

#ifndef HAVE_SYSLOG_NAMES
# include "syslognames.h"
#endif
#ifdef HAVE_SYS_KLOG_H
# include <sys/klog.h>
#endif
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "helpers.h"

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

typedef struct DuplicateTracker_ {
    char *previous_prg;
    char *previous_info;
    size_t sizeof_previous_prg;
    size_t sizeof_previous_info;
    size_t previous_sizeof_prg;
    size_t previous_sizeof_info;
    unsigned int same_counter;
} DuplicateTracker;

typedef enum FlushMode_ {
    FLUSH_DEFAULT, FLUSH_ALWAYS, FLUSH_NEVER
} FlushMode;

typedef enum LogFormat_ {
    LEGACY, LEGACY_TIMESTAMP, RFC3164, RFC5424
} LogFormat;

typedef struct {
    const char *name;
    LogFormat format;
} LogFormatMapping;

typedef struct {
    const char *name;
    int severity;
} SeverityMapping;

typedef struct RateLimiter_ {
    int64_t usec_between_msgs;
    int bucket_size;
    int token_bucket;
    int64_t last_tick;
} RateLimiter;

/* describing a remote syslog server */
typedef struct RemoteHost_ {
    char *name;                 /* internal name */
    char *hostname;             /* host name of remote host */
    char *port;                 /* UDP port */
    int sock;                   /* UDP socket to remote server */
    struct timespec last_dns;   /* time stamp of last successful DNS lookup */
    struct addrinfo *result;    /* IP address of resolved remote server */
    LogFormat format;           /* format of logging */
    int severity_level;         /* defines from which severity logs are transmitted */
    struct RemoteHost_ *next_host;
} RemoteHost;

typedef struct Output_ {
    char *directory;
    mode_t perms;
    FILE *fp;
    off_t size;
    off_t maxsize;
    int maxfiles;
    time_t maxtime;
    time_t creatime;
    DuplicateTracker dt;
    struct Output_ *next_output;
    char *postrotate_cmd;
    int showrepeats;
    const char *stamp_fmt;
    FlushMode flush;
    RateLimiter rate;
    bool compress;
} Output;

typedef enum LogLineType_ {
    LOGLINETYPE_SYSLOG,
    LOGLINETYPE_KLOG
} LogLineType;

typedef struct DataSource_ {
    LogLineType type;
    char *path;
    struct DataSource_ *next_source;
    bool required;
    int fd;
    int semantics;              /* SOCK_DGRAM or SOCK_STREAM */
    int bpos;                   /* write pointer for stream sources */
} DataSource;

typedef enum RegexSign_ {
    REGEX_SIGN_POSITIVE, REGEX_SIGN_NEGATIVE
} RegexSign;

typedef struct RegexWithSign_ {
    pcre2_code *regex;
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
    mode_t perms;
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
    int showrepeats;
    const char *stamp_fmt;
    FlushMode flush;
    int64_t usec_between_msgs;
    int burst_length;
    RemoteHost **hosts;     /* remote_hosts which should receive logs of this block */
    int num_hosts;          /* number of remote_hosts */
    LogFormat log_format;   /* format of logging */
    bool log_severity;      /* log severity level in format legacy or legacy_timestamp */
    DataSource *source;
    bool compress;
} ConfigBlock;

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
#define DEFAULT_PERMS 0700
#define OUTPUT_DIR_LOGFILES_PREFIX "log-"
#define OUTPUT_DIR_LOGFILES_SUFFIX "%Y-%m-%d-%H:%M:%S"
#define COMPRESS_SUFFIX ".gz"
#define COMPRESSED_REGEX "\\.(?:Z|gz|bz2|xz)$"
#define EXTRA_COMPRESSION_DELAY 0
#define DEFAULT_CONFIG_FILE CONFDIR "/metalog.conf"
#define DEFAULT_PID_FILE "/var/run/metalog.pid"
#define NONPRINTABLE_SUSTITUTE_CHAR '_'
#define PROGNAME_MASTER "metalog [MASTER]"
#define PROGNAME_KERNEL "metalog [KERNEL]"
#define LAST_OUTPUT "                - Last output repeated %u times -\n"
#define LAST_OUTPUT_TWICE "                - Last output repeated twice -\n"
#define MAX_SIGNIFICANT_LENGTH 512U
#define MAX_LOG_LENGTH 8192U          /* must be < (INT_MAX / 2) */
#define DEFAULT_STAMP_FMT "%b %d %T"
#define DEFAULT_UPD_PORT "514"              /* UDP port of the remote syslog server */
#define DEFAULT_DNS_LOOKUP_INTERVERVAL 120  /* seconds, after that a DNS lookup should be repeated */
#define DEFAULT_REMOTE_HOST_NAME "default"
#define DEFAULT_LOG_FORMAT 1            /* LEGACY_TIMESTAMP */
#define DEFAULT_REMOTE_LOG_FORMAT 0       /* LEGACY */
#define DEFAULT_SEVERITY_LEVEL 7        /* LOG_DEBUG */
#define NILVALUE "-"
#define BOM_UTF8 "\xEF\xBB\xBF"

#ifdef ACCEPT_UNICODE_CONTROL_CHARS
/* "DEL" (0x7f) character or all characters < "SPACE" (0x20) */
# define ISCTRLCODE(X) ((X) == 0x7f || ((unsigned char) (X)) < 0x20)
#else
/* "DEL" (0x7f) character or all characters in (extended) ASCII character control groups C0 and C1 */
# define ISCTRLCODE(X) ((X) == 0x7f || !(((unsigned char) (X)) & 0x60))
#endif

#endif
