#ifndef __METALOG_P_H__
#define __METALOG_P_H__ 1

#ifdef HAVE_KLOGCTL
# define GETOPT_OPTIONS "aBc:hps"
#else
# define GETOPT_OPTIONS "aBhps"
#endif

static struct option long_options[] = {
    { "async", 0, NULL, 'a' },    
    { "daemonize", 0, NULL, 'B' },
#ifdef HAVE_KLOGCTL
    { "consolelevel", 1, NULL, 'c' },
#endif
    { "help", 0, NULL, 'h' },
    { "pidfile", 1, NULL, 'p' },
    { "synchronous", 0, NULL, 's' },
    { "sync", 0, NULL, 's' },    
    { NULL, 0, NULL, 0 }
};

static char **argv0;
static int argv_lth;
static ConfigBlock *config_blocks;
static Output *outputs;
#ifdef HAVE_KLOGCTL
static int console_level = DEFAULT_CONSOLE_LEVEL;
#endif
static pid_t child;
static pid_t command_child;
static sig_atomic_t synchronous = (sig_atomic_t) 1;
static signed char daemonize;
static const char *pid_file = DEFAULT_PID_FILE;

#endif
