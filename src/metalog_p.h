#ifndef __METALOG_P_H__
#define __METALOG_P_H__ 1

#ifdef HAVE_KLOGCTL
# define GETOPT_OPTIONS "Bc:hsd"
#else
# define GETOPT_OPTIONS "Bhsd"
#endif

static struct option long_options[] = {
    { "daemonize", 0, NULL, 'B' },
#ifdef HAVE_KLOGCTL
    { "consolelevel", 1, NULL, 'c' },
#endif
    { "help", 0, NULL, 'h' },
    { "synchronous", 0, NULL, 's' },
    { "debug", 0, NULL, 'd' },
    { NULL, 0, NULL, 0 }
};

static char *prg_name;
static ConfigBlock *config_blocks;
static Output *outputs;
#ifdef HAVE_KLOGCTL
static int console_level = DEFAULT_CONSOLE_LEVEL;
#endif
static pid_t child;
static pid_t command_child;
static sig_atomic_t synchronous;
static signed char daemonize;

#endif
