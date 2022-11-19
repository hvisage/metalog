#include <config.h>

#define SYSLOG_NAMES 1
#include "metalog.h"
#include "metalog_p.h"
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif

static int spawn_recursion = 0;
static int dolog_queue[2];
static void signal_doLog_dequeue(void);

static int doLog(const char * fmt, ...);

/* Return values:
 *   0 - success
 *  -3 - memory failure
 * <-3 - parse error on specific lines
 *
 * Caller only cares about success; the rest are for developers while debugging
 */
static int parseLine(char * const line, ConfigBlock **cur_block,
                     const ConfigBlock * const default_block,
                     pcre2_code * const re_newblock, pcre2_code * const re_newstmt,
                     pcre2_code * const re_comment, pcre2_code * const re_null)
{
    pcre2_match_data *match_data = pcre2_match_data_create(16, NULL);
    pcre2_code *new_regex;
    char *keyword;
    char *value;
    int line_size;
    int stcount;

    if ((line_size = (int) strlen(line)) <= 0) {
        return 0;
    }
    if (line[line_size - 1] == '\n') {
        if (line_size < 2) {
            return 0;
        }
        line[--line_size] = 0;
    }
    if (pcre2_match(re_null, (PCRE2_SPTR)line, (PCRE2_SIZE)line_size,
                  0, 0, match_data, NULL) >= 0 ||
        pcre2_match(re_comment, (PCRE2_SPTR)line, (PCRE2_SIZE)line_size,
                  0, 0, match_data, NULL) >= 0) {
        return 0;
    }
    if (pcre2_match(re_newblock, (PCRE2_SPTR)line, (PCRE2_SIZE)line_size,
                  0, 0, match_data, NULL) >= 0) {
        ConfigBlock *previous_block = *cur_block;

        if ((*cur_block = wmalloc(sizeof **cur_block)) == NULL)
            return -3;
        **cur_block = *default_block;
        if (config_blocks == NULL) {
            config_blocks = *cur_block;
        } else {
            previous_block->next_block = *cur_block;
        }
        return 0;
    }
    if ((stcount =
         pcre2_match(re_newstmt, (PCRE2_SPTR)line, (PCRE2_SIZE)line_size,
                   0, 0, match_data, NULL)) >= 3) {
        PCRE2_SIZE len;
        pcre2_substring_get_bynumber(match_data, 1, (PCRE2_UCHAR **)&keyword, &len);
        pcre2_substring_get_bynumber(match_data, 2, (PCRE2_UCHAR **)&value, &len);
        if (strcasecmp(keyword, "minimum") == 0) {
            (*cur_block)->minimum = atoi(value);
        } else if (strcasecmp(keyword, "maximum") == 0) {
            (*cur_block)->maximum = atoi(value);
        } else if (strcasecmp(keyword, "facility") == 0) {
            int n = 0;

            if (*value == '*' && value[1] == 0) {
                if ((*cur_block)->facilities != NULL) {
                    free((*cur_block)->facilities);
                }
                (*cur_block)->facilities = NULL;
                (*cur_block)->nb_facilities = 0;
                return 0;
            }
            while (facilitynames[n].c_name != NULL &&
                   strcasecmp(facilitynames[n].c_name, value) != 0) {
                n++;
            }
            if (facilitynames[n].c_name == NULL) {
                warn("Unknown facility : [%s]", value);
                return -4;
            }
            (*cur_block)->facilities = wrealloc((*cur_block)->facilities,
                                                ((*cur_block)->nb_facilities + 1) *
                                                sizeof(*(*cur_block)->facilities));
            if ((*cur_block)->facilities == NULL)
                return -3;
            (*cur_block)->facilities[(*cur_block)->nb_facilities] =
                LOG_FAC(facilitynames[n].c_val);
            (*cur_block)->nb_facilities++;
        } else if (strcasecmp(keyword, "regex") == 0 ||
                   strcasecmp(keyword, "neg_regex") == 0) {
            char *regex;
            RegexWithSign *new_regexeswithsign;

            if ((regex = wstrdup(value)) == NULL)
                return -3;
            if ((new_regexeswithsign =
                 wrealloc((*cur_block)->regexeswithsign,
                          ((*cur_block)->nb_regexes + 1) *
                          sizeof *((*cur_block)->regexeswithsign))) == NULL) {
                free(regex);
                return -3;
            }
            (*cur_block)->regexeswithsign = new_regexeswithsign;
            if ((new_regex = wpcre2_compile(regex, PCRE2_CASELESS)) == NULL) {
                free(regex);
                return -5;
            }
            else {
                RegexWithSign * const this_regex =
                    &((*cur_block)->regexeswithsign[(*cur_block)->nb_regexes]);

                free(regex);
                if (strcasecmp(keyword, "neg_regex") == 0) {
                    this_regex->sign = REGEX_SIGN_NEGATIVE;
                } else {
                    this_regex->sign = REGEX_SIGN_POSITIVE;
                }
                this_regex->regex = new_regex;
            }
            (*cur_block)->nb_regexes++;
        } else if (strcasecmp(keyword, "program_regex") == 0 ||
                   strcasecmp(keyword, "program_neg_regex") == 0) {
            char *regex;
            RegexWithSign *new_regexeswithsign;

            if ((regex = wstrdup(value)) == NULL)
                return -3;
            if ((new_regexeswithsign =
                 wrealloc((*cur_block)->program_regexeswithsign,
                          ((*cur_block)->program_nb_regexes + 1) *
                          sizeof *((*cur_block)->program_regexeswithsign)))
                == NULL) {
                free(regex);
                return -3;
            }
            (*cur_block)->program_regexeswithsign = new_regexeswithsign;
            if ((new_regex = wpcre2_compile(regex, PCRE2_CASELESS)) == NULL) {
                free(regex);
                return -5;
            }
            else {
                free(regex);
                RegexWithSign * const this_regex =
                    &((*cur_block)->program_regexeswithsign
                      [(*cur_block)->program_nb_regexes]);

                if (strcasecmp(keyword, "program_neg_regex") == 0) {
                    this_regex->sign = REGEX_SIGN_NEGATIVE;
                } else {
                    this_regex->sign = REGEX_SIGN_POSITIVE;
                }
                this_regex->regex = new_regex;
            }
            (*cur_block)->program_nb_regexes++;
        } else if (strcasecmp(keyword, "maxsize") == 0) {
            (*cur_block)->maxsize = (off_t) strtoull(value, NULL, 0);
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->maxsize = (*cur_block)->maxsize;
            }
        } else if (strcasecmp(keyword, "maxfiles") == 0) {
                (*cur_block)->maxfiles = atoi(value);
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->maxfiles = (*cur_block)->maxfiles;
            }
        } else if (strcasecmp(keyword, "maxtime") == 0) {
            (*cur_block)->maxtime = (time_t) strtoull(value, NULL, 0);
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->maxtime = (*cur_block)->maxtime;
            }
        } else if (strcasecmp(keyword, "showrepeats") == 0) {
            (*cur_block)->showrepeats = atoi(value);
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->showrepeats = (*cur_block)->showrepeats;
            }
        } else if (strcasecmp(keyword, "perms") == 0) {
            (*cur_block)->perms = (mode_t) strtoul(value, NULL, 8);
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->perms = (*cur_block)->perms;
            }
        } else if (strcasecmp(keyword, "logdir") == 0) {
            char *logdir = NULL;
            Output *outputs_scan = outputs;
            Output *previous_scan = NULL;
            Output *new_output;

            while (outputs_scan != NULL) {
                if (outputs_scan->directory != NULL &&
                    strcmp(outputs_scan->directory, value) == 0) {
                    (*cur_block)->output = outputs_scan;
                    goto duplicate_output;
                }
                previous_scan = outputs_scan;
                outputs_scan = outputs_scan->next_output;
            }
            if ((new_output = wmalloc(sizeof *new_output)) == NULL ||
                (logdir = wstrdup(value)) == NULL) {
                free(new_output);
                free(logdir);
                return -3;
            }
            if (strcasecmp(value, "NONE") != 0)
                new_output->directory = logdir;
            else
                free(logdir);
            new_output->fp = NULL;
            new_output->perms = (*cur_block)->perms;
            new_output->size = (off_t) 0;
            new_output->maxsize = (*cur_block)->maxsize;
            new_output->maxfiles = (*cur_block)->maxfiles;
            new_output->maxtime = (*cur_block)->maxtime;
            new_output->postrotate_cmd = (*cur_block)->postrotate_cmd;
            new_output->showrepeats = (*cur_block)->showrepeats;
            new_output->stamp_fmt = (*cur_block)->stamp_fmt;
            new_output->flush = (*cur_block)->flush;
            new_output->dt.previous_prg = NULL;
            new_output->dt.previous_info = NULL;
            new_output->dt.sizeof_previous_prg = (size_t) 0U;
            new_output->dt.sizeof_previous_info = (size_t) 0U;
            new_output->dt.previous_sizeof_prg = (size_t) 0U;
            new_output->dt.previous_sizeof_info = (size_t) 0U;
            new_output->dt.same_counter = 0U;
            new_output->rate.usec_between_msgs = (*cur_block)->usec_between_msgs;
            new_output->rate.bucket_size = (*cur_block)->burst_length;
            new_output->rate.token_bucket = (*cur_block)->burst_length;
            new_output->rate.last_tick = 0;
            new_output->next_output = NULL;
            if (previous_scan != NULL) {
                previous_scan->next_output = new_output;
            } else {
                outputs = new_output;
            }
            (*cur_block)->output = new_output;
            duplicate_output:
            (void) 0;
        } else if (strcasecmp(keyword, "command") == 0) {
            if (((*cur_block)->command = wstrdup(value)) == NULL)
                return -3;
        } else if (strcasecmp(keyword, "program") == 0) {
            if (((*cur_block)->program = wstrdup(value)) == NULL)
                return -3;
        } else if (strcasecmp(keyword, "postrotate_cmd") == 0) {
            if (((*cur_block)->postrotate_cmd = wstrdup(value)) == NULL)
               return -3;
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->postrotate_cmd = (*cur_block)->postrotate_cmd;
            }
        } else if (strcasecmp(keyword, "remote_host") == 0) {
            if ((remote_host.hostname = wstrdup(value)) == NULL)
               return -3;
        } else if (strcasecmp(keyword, "remote_port") == 0) {
            if ((remote_host.port = wstrdup(value)) == NULL)
               return -3;
        } else if (strcasecmp(keyword, "remote_log") == 0) {
            (*cur_block)->remote_log = atoi(value);
        } else if (strcasecmp(keyword, "break") == 0) {
            (*cur_block)->brk = atoi(value);
        } else if (strcasecmp(keyword, "stamp_fmt") == 0) {
            if (((*cur_block)->stamp_fmt = wstrdup(value)) == NULL)
               return -3;
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->stamp_fmt = (*cur_block)->stamp_fmt;
            }
        } else if (strcasecmp(keyword, "flush") == 0) {
            if (atoi(value))
                (*cur_block)->flush = FLUSH_ALWAYS;
            else
                (*cur_block)->flush = FLUSH_NEVER;
            if ((*cur_block)->output != NULL)
                (*cur_block)->output->flush = (*cur_block)->flush;
        } else if (strcasecmp(keyword, "ratelimit") == 0) {
            double base_rate;
            int64_t usec_between_msgs;
            char *end;

            base_rate = strtod(value, &end);
            if (end == value) {
                warnp("Missing number : [%s]", value);
                return -6;
            }
            if (base_rate < 0.) {
                warn("Negative number : [%s]", value);
                return -6;
            }

            if (base_rate) {
                double usec;
                if (end[0] != '/') {
                    warn("Missing unit of time : [%s]", value);
                    return -6;
                }
                switch (end[1]) {
                case 's': usec = 1000000.; break;
                case 'm': usec = 1000000. * 60; break;
                case 'h': usec = 1000000. * 60 * 60; break;
                case 'd': usec = 1000000. * 60 * 60 * 24; break;
                default:
                    warn("Invalid unit of time : [%s]", end + 1);
                    return -6;
                }
                usec_between_msgs = usec / base_rate;
            } else
                usec_between_msgs = base_rate;

            assert(usec_between_msgs >= 0);
            (*cur_block)->usec_between_msgs = usec_between_msgs;
            if ((*cur_block)->output != NULL)
                (*cur_block)->output->rate.usec_between_msgs =
                    usec_between_msgs;
        } else if (strcasecmp(keyword, "ratelimit_burst") == 0) {
            int burst_length = atoi(value);
            if (burst_length < 1) {
                warn("Non-positive bust length : [%s]", value);
                return -6;
            }
            (*cur_block)->burst_length = burst_length;
            if ((*cur_block)->output != NULL) {
                (*cur_block)->output->rate.bucket_size = burst_length;
                (*cur_block)->output->rate.token_bucket = burst_length;
            }
        } else
            err("Unknown keyword '%s'! line: %s", keyword, line);
        pcre2_substring_free((PCRE2_UCHAR *)keyword);
        pcre2_substring_free((PCRE2_UCHAR *)value);
    }
    pcre2_match_data_free(match_data);
    return 0;
}

static int configParser(const char * const file)
{
    char line[LINE_MAX];
    FILE *fp;
    pcre2_code *re_newblock;
    pcre2_code *re_newstmt;
    pcre2_code *re_comment;
    pcre2_code *re_null;
    int retcode = 0;
    ConfigBlock default_block = {
            DEFAULT_MINIMUM,           /* minimum */
            DEFAULT_MAXIMUM,           /* maximum */
            NULL,                      /* facilities */
            0,                         /* nb_facilities */
            NULL,                      /* regexes */
            0,                         /* nb_regexes */
            (off_t) DEFAULT_MAXSIZE,   /* maxsize */
            DEFAULT_MAXFILES,          /* maxfiles */
            (time_t) DEFAULT_MAXTIME,  /* maxtime */
            (mode_t) DEFAULT_PERMS,    /* perms */
            NULL,                      /* output */
            NULL,                      /* command */
            NULL,                      /* program */
            0,                         /* break flag */
            NULL,                      /* program_regexes */
            0,                         /* program_nb_regexes */
            NULL,                      /* next_block */
            NULL,                      /* postrotate_cmd */
            0,                         /* showrepeats */
            DEFAULT_STAMP_FMT,         /* stamp_fmt */
            FLUSH_DEFAULT,             /* flush */
            0,                         /* usec_between_msgs */
            1,                         /* max_burst_length */
            false,                     /* send log entry to remote logger */
    };
    ConfigBlock *cur_block = &default_block;

    if ((fp = fopen(file, "r")) == NULL) {
        warnp("Can't open the configuration file");
        return -2;
    }
    re_newblock = wpcre2_compile(":\\s*$", 0);
    re_newstmt = wpcre2_compile("^\\s*(.+?)\\s*=\\s*\"?([^\"]*)\"?\\s*$", 0);
    re_comment = wpcre2_compile("^\\s*#", 0);
    re_null = wpcre2_compile("^\\s*$", 0);
    if (!re_newblock || !re_newstmt || !re_comment || !re_null) {
        retcode = -1;
        goto rtn;
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        if ((retcode = parseLine(line, &cur_block, &default_block,
                                 re_newblock, re_newstmt,
                                 re_comment, re_null)) != 0) {
            break;
        }
    }
 rtn:
    if (re_newblock != NULL) {
        pcre2_code_free(re_newblock);
    }
    if (re_newstmt != NULL) {
        pcre2_code_free(re_newstmt);
    }
    if (re_comment != NULL) {
        pcre2_code_free(re_comment);
    }
    if (re_null != NULL) {
        pcre2_code_free(re_null);
    }
    fclose(fp);

    return retcode;
}

static void clearargs(int argc, char **argv)
{
#ifndef NO_PROCNAME_CHANGE
# if defined(__linux__) && !defined(HAVE_SETPROCTITLE)
    int i;

    for (i = 0; environ[i] != NULL; i++);
    argv0 = argv;
    if (i > 0) {
        argv_lth = environ[i-1] + strlen(environ[i-1]) - argv0[0];
    } else {
        argv_lth = argv0[argc-1] + strlen(argv0[argc-1]) - argv0[0];
    }
    if (environ != NULL) {
        char **new_environ;
        unsigned int env_nb = 0U;

        while (environ[env_nb] != NULL) {
            env_nb++;
        }
        if ((new_environ = malloc((1U + env_nb) * sizeof (char *))) == NULL) {
            abort();
        }
        new_environ[env_nb] = NULL;
        while (env_nb > 0U) {
            env_nb--;
            /* Can any bad thing happen if strdup() ever fails? */
            new_environ[env_nb] = xstrdup(environ[env_nb]);
        }
        environ = new_environ;
    }
# else
    (void) argc;
    (void) argv;
# endif
#endif
}

#ifndef HAVE_SETPROGNAME
static void setprogname(const char * const title)
{
#ifndef NO_PROCNAME_CHANGE
# ifdef HAVE_SETPROCTITLE
    setproctitle("-%s", title);
# elif defined(__linux__)

    if (argv0 != NULL) {
        memset(argv0[0], 0, argv_lth);
        strncpy(argv0[0], title, argv_lth - 2);
        argv0[1] = NULL;
    }
# elif defined(__hpux__)
    union pstun pst;

    pst.pst_command = title;
    pstat(PSTAT_SETCMD, pst, strlen(title), 0, 0);
# endif
#endif
    (void) title;
}
#endif

static int getDataSources(int sockets[])
{
    struct sockaddr_un sa;
#ifdef HAVE_KLOGCTL
    int fdpipe[2];
    pid_t pgid;
    int loop = 1;
#endif

    if ((sockets[0] = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0) {
        warnp("Unable to create a local socket");
        return -1;
    }
    sa.sun_family = AF_UNIX;
    if (snprintf(sa.sun_path, sizeof sa.sun_path, "%s", SOCKNAME) < 0) {
        warnp("Socket name too long");
        close(sockets[0]);
        return -2;
    }
    unlink(sa.sun_path);
    if (bind(sockets[0], (struct sockaddr *) &sa, (socklen_t) sizeof sa) < 0) {
        warnp("Unable to bind a local socket");
        close(sockets[0]);
        return -1;
    }
    chmod(sa.sun_path, 0666);
    sockets[1] = -1;

    /* setup the signal handler pipe */
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, dolog_queue) < 0) {
        if (pipe(dolog_queue) < 0) {
            warnp("Unable to create a pipe");
            return -4;
        }
    }

    if (!do_kernel_log) {
        /*
         * This will avoid reading from the kernel socket in the process()
         * function, which only takes into account valid descriptors.
         */
        sockets[1] = -1;
        return 0;
    }

#ifdef HAVE_KLOGCTL
    /* larger buffers compared to a pipe() */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fdpipe) < 0) {
        warnp("Unable to create a pipe");
        close(sockets[0]);
        return -3;
    }
    pgid = getpgrp();
    if ((child = fork()) < (pid_t) 0) {
        warnp("Unable to create the klogd child");
        close(sockets[0]);
        return -3;
    } else if (child == (pid_t) 0) {
        char line[LINE_MAX];
        int s;

        signal(SIGUSR1, SIG_IGN);
        signal(SIGUSR2, SIG_IGN);
        setpgid((pid_t) 0, pgid);
        setprogname(PROGNAME_KERNEL);
        if (klogctl(1, NULL, 0) < 0 ||
            klogctl(8, NULL, console_level) < 0 ||
            klogctl(6, NULL, 0) < 0) {
            warnp("Unable to control the kernel logging device");
            return -4;
        }

        while (loop) {
            while ((s = klogctl(2, line, sizeof (line))) < 0 && errno == EINTR)
                continue;
            if (s == -1) {
                loop = 0;
                break;
            }
            /* make sure we write the whole buffer into the pipe
               line parsing is done on the other end */
            int out = 0;
            while (out < s) {
                int w = write(fdpipe[1], &line[out], s - out);
                if (w == -1 && errno != EINTR) {
                    loop = 0;
                    break;
                }
                out += w;
            }
        }
        klogctl(7, NULL, 0);
        klogctl(0, NULL, 0);

        return 0;
    }
    sockets[1] = fdpipe[0];
#else                                  /* !HAVE_KLOGCTL */
    {
        int klogfd;

        if ((klogfd = open(KLOG_FILE, O_RDONLY)) < 0) {
            return 0;                  /* non-fatal */
        }
        sockets[1] = klogfd;
    }
#endif

    return 0;
}

static int spawnCommand(const char * const command, const char * const date,
                        const char * const prg, const char * const info)
{
    pid_t pid;

    if (spawn_recursion) return 1;
    spawn_recursion++;

    pid = fork();
    if (pid == 0) {
        execl(command, command, date, prg, info, (char *) NULL);
        _exit(127);
    }

    if (verbose)
        doLog("Forked command \"%s \"%s\" \"%s\" \"%s\" [%u].",
              command, date, prg, info, (unsigned) pid);
    spawn_recursion--;
    return 0;
}

static int parseLogLine(const LogLineType loglinetype, char *line,
                        int * const logcode,
                        const char ** const prg, char ** const info)
{
#ifndef HAVE_KLOGCTL
    if (loglinetype != LOGLINETYPE_KLOG) {
#endif
        if (*line != '<') {
            return -1;
        }
        line++;
        if ((*logcode = atoi(line)) <= 0) {
            return -1;
        }
        while (*line != '>') {
            if (*line == 0) {
                return -1;
            }
            line++;
        }
        line++;
#ifndef HAVE_KLOGCTL
    } else {
        *logcode = 0;
    }
#endif

    if (loglinetype == LOGLINETYPE_KLOG) {
        *logcode |= LOG_KERN;
        *prg = CF_PROGNAME_KERNEL;
        *info = line;
        return 0;
    }
    while (*line != ':') {
        if (*line == 0) {
            return -1;
        }
        line++;
    }
    line++;
    while (isspace(*line) == 0) {
        if (*line == 0) {
            return -1;
        }
        line++;
    }
    *line++ = 0;
    *prg = line;
    while (*line != '[' && *line != ':') {
        if (*line == 0) {
            return -1;
        }
        line++;
    }
    if (*line == '[') {
        *line++ = 0;
        while (*line != ']') {
            if (*line == 0) {
                return -1;
            }
            line++;
        }
        line++;
        while (*line != ':') {
            if (*line == 0) {
                return -1;
            }
            line++;
        }
    } else {
        *line = 0;
    }
    line++;
    while (isspace(*line) != 0) {
        if (*line == 0) {
            return -1;
        }
        line++;
    }
    *info = line;

    return 0;
}

static int rotateLogFiles(const char * const directory, const int maxfiles)
{
    char path[PATH_MAX];
    char old_name[PATH_MAX];
    const char *name;
    DIR *dir;
    struct dirent *dirent;
    int foundlogs;
    int year, mon, mday, hour, min, sec;
    int older_year, older_mon = INT_MAX, older_mday = INT_MAX,
        older_hour = INT_MAX, older_min = INT_MAX, older_sec = INT_MAX;

    rescan:
    foundlogs = 0;
    *old_name = 0;
    older_year = INT_MAX;
    if ((dir = opendir(directory)) == NULL) {
        warnp("Unable to rotate [%s]", directory);
        return -1;
    }
    while ((dirent = readdir(dir)) != NULL) {
        name = dirent->d_name;
        if (strncmp(name, OUTPUT_DIR_LOGFILES_PREFIX,
                    sizeof OUTPUT_DIR_LOGFILES_PREFIX - 1U) == 0) {
            if (sscanf(name, OUTPUT_DIR_LOGFILES_PREFIX "%d-%d-%d-%d:%d:%d",
                       &year, &mon, &mday, &hour, &min, &sec) != 6) {
                continue;
            }
            foundlogs++;
            if (year < older_year || (year == older_year &&
               (mon  < older_mon  || (mon  == older_mon  &&
               (mday < older_mday || (mday == older_mday &&
               (hour < older_hour || (hour == older_hour &&
               (min  < older_min  || (min  == older_min  &&
               (sec  < older_sec))))))))))) {   /* yeah! */
                older_year = year;
                older_mon = mon;
                older_mday = mday;
                older_hour = hour;
                older_min = min;
                older_sec = sec;
                strncpy(old_name, name, sizeof old_name);
                old_name[sizeof old_name - 1U] = 0;
            }
        }
    }
    closedir(dir);
    if (foundlogs >= maxfiles) {
        if (*old_name == 0) {
            return -3;
        }
        if (snprintf(path, sizeof path, "%s/%s", directory, old_name) < 0) {
            warnp("Path too long to unlink [%s/%s]", directory, old_name);
            return -4;
        }
        if (unlink(path) < 0) {
            return -2;
        }
        foundlogs--;
        if (foundlogs >= maxfiles) {
            goto rescan;
        }
    }

    return 0;
}

static int rateLimit(RateLimiter * const rl)
{
    struct timeval tv;
    int64_t current_tick, diff_ticks;

    if (rl->usec_between_msgs == 0)
        return 0; /* output channel is not limited */
    if (gettimeofday(&tv, NULL))
        return 0; /* if in doubt, never limit */

    /* Fill up bucket with one token every usec_between_msgs microseconds */
    current_tick = tv.tv_sec * (int64_t)1000000 + tv.tv_usec;
    current_tick /= rl->usec_between_msgs;
    diff_ticks = current_tick - rl->last_tick;
    if (diff_ticks > 0) {
        diff_ticks += rl->token_bucket;
        if (diff_ticks < rl->bucket_size)
            rl->token_bucket = (int)diff_ticks;
        else /* bucket overflow */
            rl->token_bucket = rl->bucket_size;
    }
    rl->last_tick = current_tick;

    /* Log message only if token available to pay for it */
    if (rl->token_bucket <= 0)
        return 1; /* suppress this message due to rate limit */
    --rl->token_bucket;
    return 0; /* message is within limit */
}

static int writeLogLine(Output * const output, const char * const date,
                        const char * const prg, const char * const info)
{
    size_t sizeof_prg;
    size_t sizeof_info;
    time_t now;

    if (output == NULL || output->directory == NULL ||
        (sizeof_prg = strlen(prg)) <= (size_t) 0U ||
        (sizeof_info = strlen(info)) <= (size_t) 0U) {
        return 0;
    }
    if (sizeof_info > MAX_SIGNIFICANT_LENGTH) {
        sizeof_info = MAX_SIGNIFICANT_LENGTH;
    }
    if (sizeof_prg > MAX_SIGNIFICANT_LENGTH) {
        sizeof_prg = MAX_SIGNIFICANT_LENGTH;
    }

    if (rateLimit(&output->rate)) {
        return 0;
    }
    if (output->showrepeats == 0 &&
        sizeof_info == output->dt.previous_sizeof_info &&
        sizeof_prg == output->dt.previous_sizeof_prg &&
        memcmp(output->dt.previous_info, info, sizeof_info) == 0 &&
        memcmp(output->dt.previous_prg, prg, sizeof_prg) == 0) {
        if (output->dt.same_counter < UINT_MAX) {
            output->dt.same_counter++;
        }
        return 0;
    }

    if (sizeof_info > output->dt.sizeof_previous_info) {
        char *pp = output->dt.previous_info;

        if ((pp = wrealloc(output->dt.previous_info, sizeof_info)) != NULL) {
            output->dt.previous_info = pp;
            memcpy(output->dt.previous_info, info, sizeof_info);
            output->dt.sizeof_previous_info = sizeof_info;
        }
    } else {
        memcpy(output->dt.previous_info, info, sizeof_info);
        output->dt.previous_sizeof_info = sizeof_info;
    }
    if (sizeof_prg > output->dt.sizeof_previous_prg) {
        char *pp = output->dt.previous_prg;

        if ((pp = wrealloc(output->dt.previous_prg, sizeof_prg)) != NULL) {
            output->dt.previous_prg = pp;
            memcpy(output->dt.previous_prg, prg, sizeof_prg);
            output->dt.sizeof_previous_prg = sizeof_prg;
        }
    } else {
        memcpy(output->dt.previous_prg, prg, sizeof_prg);
        output->dt.previous_sizeof_prg = sizeof_prg;
    }
    now = time(NULL);
    if (output->fp == NULL) {
        struct stat st;
        FILE *fp;
        FILE *fp_ts;
        time_t creatime;
        char path[PATH_MAX];

        testdir:
        if (stat(output->directory, &st) < 0) {
            if (mkdir(output->directory, output->perms) < 0) {
                warnp("Can't create [%s]", output->directory);
                return -1;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            if (unlink(output->directory) < 0) {
                warnp("Can't unlink [%s]", output->directory);
                return -1;
            }
            goto testdir;
        }
        if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_CURRENT,
                     output->directory) < 0) {
            warnp("Path name too long for current in [%s]", output->directory);
            return -2;
        }
        if ((fp = fopen(path, "a")) == NULL) {
            warnp("Unable to access [%s]", path);
            return -3;
        }
        if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_TIMESTAMP,
                     output->directory) < 0) {
            warnp("Path name too long for timestamp in [%s]", output->directory);
            fclose(fp);
            return -2;
        }
        if ((fp_ts = fopen(path, "r")) == NULL) {
            recreate_ts:
            creatime = time(NULL);
            if ((fp_ts = fopen(path, "w")) == NULL) {
                warnp("Unable to write the timestamp [%s]", path);
                fclose(fp);
                return -3;
            }
            fprintf(fp_ts, "%llu\n", (unsigned long long) creatime);
        } else {
            unsigned long long creatime_;

            if (fscanf(fp_ts, "%llu", &creatime_) <= 0) {
                fclose(fp_ts);
                goto recreate_ts;
            }
            creatime = (time_t) creatime_;
        }
        if (fclose(fp_ts) != 0) {
            warnp("Unable to close [%s]", path);
            return -3;
        }
        output->creatime = creatime;
        if (output->maxtime)
            output->creatime -= (creatime % output->maxtime);
        output->size = (off_t) ftell(fp);
        output->fp = fp;
    }
    if ((output->maxsize != 0) && (output->maxtime != 0)) {
        if (output->size >= output->maxsize ||
            now > (output->creatime + output->maxtime)) {
            struct tm *time_gm;
            char path[PATH_MAX];
            char newpath[PATH_MAX];

            if (output->fp == NULL) {
                warnf("Internal inconsistency");
                return -6;
            }
            if ((time_gm = gmtime(&now)) == NULL) {
                warnp("Unable to find the current date");
                return -4;
            }
            if (snprintf(newpath, sizeof newpath, "%s/" OUTPUT_DIR_LOGFILES_PREFIX,
                output->directory) < 0)
            {
                warnp("Path name too long for new path in [%s]", output->directory);
                return -2;
            }
            if (strftime(newpath + strlen(newpath), sizeof newpath - strlen(newpath),
                        OUTPUT_DIR_LOGFILES_SUFFIX, time_gm) == 0)
            {
                warnp("Path name too long for new path in [%s]", output->directory);
                return -2;
            }
            if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_CURRENT,
                        output->directory) < 0) {
                warnp("Path name too long for current in [%s]", output->directory);
                return -2;
            }
            rotateLogFiles(output->directory, output->maxfiles);
            fclose(output->fp);
            output->fp = NULL;
            if (rename(path, newpath) < 0 && unlink(path) < 0) {
                warnp("Unable to rename [%s] to [%s]", path, newpath);
                return -5;
            }
            if (output->postrotate_cmd != NULL)
                spawnCommand(output->postrotate_cmd, date, prg, newpath);
            if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_TIMESTAMP,
                        output->directory) < 0) {
                warnp("Path name too long for timestamp in [%s]", output->directory);
                return -2;
            }
            unlink(path);
            goto testdir;
        }
    }
    if (output->dt.same_counter > 0U) {
        if (output->dt.same_counter == 1) {
            fprintf(output->fp, LAST_OUTPUT_TWICE);
            output->size += (off_t) (sizeof LAST_OUTPUT_TWICE - (size_t) 1U);
        } else {
            int fps;

            fps = fprintf(output->fp, LAST_OUTPUT, output->dt.same_counter);
            if (fps >= (int) (sizeof LAST_OUTPUT - sizeof "%u")) {
                output->size += (off_t) fps;
            } else if (fps > 0) {
                output->size += (off_t) (sizeof LAST_OUTPUT + (size_t) 10U);
            }
        }
        output->dt.same_counter = 0U;
    }
    if (date[0])
        fprintf(output->fp, "%s ", date);
    fprintf(output->fp, "[%s] %s\n", prg, info);
    output->size += (off_t) strlen(date);
    output->size += (off_t) (sizeof_prg + sizeof_info);
    output->size += (off_t) 5;
    if (output->flush == FLUSH_ALWAYS ||
        (output->flush == FLUSH_DEFAULT && synchronous != (sig_atomic_t) 0)) {
        fflush(output->fp);
    }
    return 0;
}

/* send a line to a remote syslog server */
static int sendRemote(const char * const prg, const char * const info)
{
    struct timespec now;
    struct addrinfo hints;
    struct addrinfo *rp;
    char *line = NULL;
    int ret = 0;

    /* prepare log entry */
    if ((line = wmalloc(strlen("  []  ") + strlen(prg) + strlen(info) + 1)) == NULL)
        return -1;
    sprintf(line, "[%s] %s\n", prg, info);

    /* everything seems to be ready to send to remote host immediatelly */
    clock_gettime(CLOCK_MONOTONIC, &now);
    if ((remote_host.sock > 0) &&
        ((remote_host.last_dns.tv_sec + DEFAULT_DNS_LOOKUP_INTERVERVAL) > now.tv_sec)) {
        if (sendto(remote_host.sock, line, strlen(line), 0, remote_host.result->ai_addr, remote_host.result->ai_addrlen) == -1) {
            close(remote_host.sock);
            remote_host.sock = -1;
        }
        else {
            free(line);
            return 0;
        }
    }

    /* sending failed or DNS info is too old, try again after updated DNS */
    clock_gettime(CLOCK_MONOTONIC, &remote_host.last_dns);
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
    hints.ai_socktype = SOCK_DGRAM;
    if (remote_host.result != NULL) {
        freeaddrinfo(remote_host.result);
        remote_host.result = NULL;
    }
    if (getaddrinfo(remote_host.hostname, remote_host.port, &hints, &remote_host.result) != 0) {
        free(line);
        return -1;
    }

    /* close an eventually open socket */
    if (remote_host.sock > 0)
        close(remote_host.sock);

    /* establish the socket to the remote host using all its resolved addresses */
    for (rp = remote_host.result; rp != NULL; rp = rp->ai_next) {
        if (remote_host.sock <= 0)
            remote_host.sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (remote_host.sock < 0)
            continue;

        /* try to send the line */
        if (sendto(remote_host.sock, line, strlen(line), 0, rp->ai_addr, rp->ai_addrlen) == -1) {
            close(remote_host.sock);
            remote_host.sock = -1;
            continue;
        }
        else {
            break;
        }
    }
    free(line);

    if(!rp)
        ret = -1;

    return ret;
}

static void flushAll(void)
{
    ConfigBlock *block = config_blocks;
    while (block) {
        if (block->output && block->output->fp)
            fflush_unlocked(block->output->fp);
        block = block->next_block;
    }
}

static int get_stamp_fmt_timestamp(const char *stamp_fmt, char *datebuf, int datebuf_size)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm = localtime(&ts.tv_sec);

    if ((!stamp_fmt) || (!datebuf) || datebuf_size < 1) {
        return -1;
    }

    if (tm) {
        /* make a copy as we may change the string */
        char *fmt_str = strdup(stamp_fmt);
        /* Search for and process "%N" */
        char *p = fmt_str;
        while ((p = strchr(p, '%')) != NULL) {
            char *tmp = NULL;
            int n, m;
            unsigned scale;
            unsigned long precision;
            p++;
            if (*p == '%') {
                p++;
                continue;
            }
            n = strspn(p, "0123456789");
            if (p[n] != 'N') {
                p += n;
                continue;
            }
            /* We have "%[nnn]N" */
            p[-1] = '\0';
            p[n] = '\0';
            scale = 1;
            precision = 9;
            if (n) {
                int old_errno = errno;
                char *e = NULL;
                errno = 0;
                precision = strtoul(p, &e, 10);
                if (!errno && p != e && !*e) {
                    if (precision == 0 || precision > 9)
                        precision = 9;
                    m = 9 - precision;
                    while (--m >= 0)
                        scale *= 10;
                }
                errno = old_errno;
            }
            m = p - fmt_str;
            p += n + 1;
            if (asprintf(&tmp, "%s%0*u%s", fmt_str, (unsigned)precision, (unsigned)ts.tv_nsec / scale, p) <= 0) {
                break;
            }
            free(fmt_str);
            fmt_str = tmp;
            p = fmt_str + m;
        }
        strftime(datebuf, datebuf_size, fmt_str, tm);
        free(fmt_str);
    }

    return 0;
}

static int log_stdout(const char * const date,
                      const char * const prg, const char * const info)
{
    printf("%s [%s] %s\n", date, prg, info);
    return 0;
}

static int processLogLine(const int logcode,
                          const char * const prg, char * const info)
{
    pcre2_match_data *match_data = pcre2_match_data_create(16, NULL);
    ConfigBlock *block = config_blocks;
    RegexWithSign *this_regex;
    const int facility = LOG_FAC(logcode);
    const int priority = LOG_PRI(logcode);
    int nb_regexes;
    int info_len;
    int prg_len;
    int regex_result;

    while (block != NULL) {
        if (block->facilities != NULL) {
            int nb = block->nb_facilities;
            const int * const facilities = block->facilities;

            while (nb > 0) {
                nb--;
                if (facility == facilities[nb]) {
                    goto facility_ok;
                }
            }
            goto nextblock;
        }
        facility_ok:
        if (priority > block->minimum && priority < block->maximum) {
            goto nextblock;
        }
        if (block->program != NULL && strcasecmp(block->program, prg) != 0) {
            goto nextblock;
        }
        if ((nb_regexes = block->program_nb_regexes) > 0) {
            regex_result = 0;
            if ((prg_len = (int) strlen(prg)) < 0) {
                goto nextblock;
            }
            this_regex = block->program_regexeswithsign;
            do {
                if (this_regex->sign == REGEX_SIGN_POSITIVE) {
                    if (pcre2_match(this_regex->regex,
                                    (PCRE2_SPTR)prg, (PCRE2_SIZE)prg_len,
                                    0, 0, match_data, NULL) >= 0) {
                        /* pass, unless a program_neg_regex matches */
                        regex_result = 1;
                    } else if (regex_result != 1) {
                        /* fail, unless another program_regex matches */
                        regex_result = -1;
                    }
                } else {
                    if (pcre2_match(this_regex->regex,
                                    (PCRE2_SPTR)prg, (PCRE2_SIZE)prg_len,
                                    0, 0, match_data, NULL) >= 0) {
                        /* fail immediately */
                        goto nextblock;
                    }
                    /* pass, unless a non-matching program_regex is found or another program_neg_regex matches */
                }
                this_regex++;
                nb_regexes--;
            } while (nb_regexes > 0);
            if (regex_result == -1) {
                goto nextblock;
            }
        }
        if ((info_len = (int) strlen(info)) <= 0) {
            goto nextblock;
        }
        if ((nb_regexes = block->nb_regexes) > 0 && *info != 0) {
            regex_result = 0;
            this_regex = block->regexeswithsign;
            do {
                if (this_regex->sign == REGEX_SIGN_POSITIVE) {
                    if (pcre2_match(this_regex->regex,
                                    (PCRE2_SPTR)info, (PCRE2_SIZE)info_len,
                                    0, 0, match_data, NULL) >= 0) {
                        /* pass, unless a neg_regex matches */
                        regex_result = 1;
                    } else if (regex_result != 1) {
                        /* fail, unless another regex matches */
                        regex_result = -1;
                    }
                } else {
                    if (pcre2_match(this_regex->regex,
                                    (PCRE2_SPTR)info, (PCRE2_SIZE)info_len,
                                    0, 0, match_data, NULL) >= 0) {
                        /* fail immediately */
                        goto nextblock;
                    }
                    /* pass, unless a non-matching regex is found or another neg_regex matches */
                }
                this_regex++;
                nb_regexes--;
            } while (nb_regexes > 0);
            if (regex_result == -1) {
                goto nextblock;
            }
        }
        if ((size_t) info_len > MAX_LOG_LENGTH) {
            info[MAX_LOG_LENGTH] = 0;
        }

        char datebuf[100] = { '\0' };
        if (block->output && block->output->stamp_fmt[0]) {
            get_stamp_fmt_timestamp(block->output->stamp_fmt,
                                    datebuf,
                                    sizeof(datebuf));
        }

        bool do_break = false;
        if (block->output != NULL) {
            /* write a real log entry */
            writeLogLine(block->output, datebuf, prg, info);

            /* write to stdout */
            log_stdout(datebuf, prg, info);

            /* send the log entry to the remote host */
            if (remote_host.hostname != NULL && block->remote_log) {
                sendRemote(prg, info);
            }

            if (block->brk)
                do_break = true;
        }
        if (block->command != NULL) {
            spawnCommand(block->command, datebuf, prg, info);
            if (block->brk)
                do_break = true;
        }
        if (do_break)
            break;

        nextblock:

        block = block->next_block;
    }

    pcre2_match_data_free(match_data);
    return 0;
}

static int doLog(const char * fmt, ...)
{
    char infobuf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf (infobuf, sizeof infobuf, fmt, ap);
    va_end(ap);

    return processLogLine(LOG_SYSLOG, "metalog", infobuf);
}

static void sanitize(char * const line_)
{
    register unsigned char *line = (unsigned char *) line_;

    while (*line != 0U) {
        if (ISCTRLCODE(*line)) {
            *line = NONPRINTABLE_SUSTITUTE_CHAR;
        }
        line++;
    }
}

static int log_line(LogLineType loglinetype, char *buf)
{
    int logcode;
    const char *prg;
    char *info;

    sanitize(buf);
    if (parseLogLine(loglinetype, buf, &logcode, &prg, &info) < 0)
      return -1;
    return processLogLine(logcode, prg, info);
}

static int log_kernel(char *buf, int bsize)
{
    char *s = buf;
    int n = 0, start = 0;

    s = buf;
    while (n < bsize) {
        if (s[n] == '\n') {
            s[n] = '\0';
            log_line(LOGLINETYPE_KLOG, &s[start]);
            start = n + 1;
        }
        n++;
    }

    /* we couldn't find any \n ???
       => invalidate this buffer */
    if (start == 0)
      return 0;

    return bsize - start;
}

static int process(const int sockets[])
{
    struct pollfd fds[3], *siglog, *syslog, *klog;
    int nfds;
    int event;
    ssize_t rd;
    int ld;
    char buffer[2][4096];
    int bpos;

    siglog = syslog = klog = NULL;
    nfds = 0;

    siglog = &fds[nfds];
    fds[nfds].fd = dolog_queue[0];
    fds[nfds].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
    fds[nfds].revents = 0;
    ++nfds;
    if (sockets[0] >= 0) {
        syslog = &fds[nfds];
        fds[nfds].fd = sockets[0];
        fds[nfds].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
        fds[nfds].revents = 0;
        ++nfds;
    }
    if (sockets[1] >= 0) {
        klog = &fds[nfds];
        fds[nfds].fd = sockets[1];
        fds[nfds].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
        fds[nfds].revents = 0;
        ++nfds;
    }

    bpos = 0;
    for (;;) {
        while (poll(fds, nfds, -1) < 0 && errno == EINTR)
            ;

        /* Signal queue */
        if (siglog && siglog->revents) {
            event = siglog->revents;
            siglog->revents = 0;
            if (event != POLLIN) {
                warn("Signal queue socket error - aborting");
                close(siglog->fd);
                return -1;
            }

            signal_doLog_dequeue();
        }

        /* UDP socket (syslog) */
        if (syslog && syslog->revents) {
            event = syslog->revents;
            syslog->revents = 0;
            if (event != POLLIN) {
                warn("Syslog socket error - aborting");
                close(syslog->fd);
                return -1;
            }

            /* receive a single log line (UDP) and process it. receive one byte for '\0' */
            while (((rd = read(syslog->fd, buffer[0], sizeof(buffer[0])-1)) < 0) && (errno == EINTR))
                ;
            if (rd == -1)
                return -1;

            buffer[0][rd] = '\0';
            log_line(LOGLINETYPE_SYSLOG, buffer[0]);
        }

        /* STREAM_SOCKET (klog) */
        if (klog && klog->revents) {
            event = klog->revents;
            klog->revents = 0;
            if (event != POLLIN) {
                warn("Klog socket error - aborting");
                close(klog->fd);
                return -1;
            }

            /* receive a chunk of kernel log message... */
            while ((rd = read(klog->fd, &buffer[1][bpos], sizeof(buffer[1]) - bpos)) < 0 && errno == EINTR)
                ;
            if (rd == -1)
                return -1;

            /* ... and process them line by line */
            rd += bpos;
            if ((ld = log_kernel(buffer[1], rd)) > 0) {
                /* move remainder of a message to the beginning of the buffer
                   it'll be logged once we can read the whole line into buffer[1] */
                memmove(buffer[1], &buffer[1][ld], bpos = rd - ld);
            } else {
                bpos = 0;
            }
        }
    }
    return 0;
}

static int delete_pid_file(const char * const pid_file)
{
    return pid_file ? unlink(pid_file) : 0;
}

static int update_pid_file(const char * const pid_file)
{
    int fd;
    FILE *fp;

    if (pid_file == NULL || *pid_file != '/')
        return 0;

    fd = open(pid_file, O_CREAT | O_WRONLY | O_TRUNC | O_NOFOLLOW, 0644);
    if (fd == -1)
        goto err;
    if (fchmod(fd, 0644))
        goto err;

    fp = fdopen(fd, "w");
    if (!fp)
        goto err;
    if (fprintf(fp, "%lu\n", (unsigned long)getpid()) <= 0)
        goto err;
    if (ferror(fp))
        goto err;
    if (fclose(fp))
        goto err;

    return 0;

 err:
    warnp("pid file creation failed: %s", pid_file);
    return -1;
}

static void exit_hook(void)
{
    /* Flush all buffers just to be sure */
    synchronous = (sig_atomic_t) 1;
    flushAll();
    if (child > (pid_t) 0) {
        kill(child, SIGTERM);
    }
    delete_pid_file(pid_file);
}

static void signal_doLog_queue(const char *fmt, const unsigned int pid, const int status)
{
    ssize_t ret;
    unsigned int fmt_len = (unsigned int)strlen(fmt);
    char buf[sizeof(pid) + sizeof(status) + sizeof(fmt_len)];
    memcpy(buf, &pid, sizeof(pid));
    memcpy(buf+sizeof(pid), &status, sizeof(status));
    memcpy(buf+sizeof(pid)+sizeof(status), &fmt_len, sizeof(fmt_len));
    ret = write(dolog_queue[1], buf, sizeof(buf));
    assert(ret == sizeof(buf));
    ret = write(dolog_queue[1], fmt, fmt_len);
    assert(ret == (ssize_t) fmt_len);
}

static void signal_doLog_dequeue(void)
{
    unsigned int pid, fmt_len;
    int status;
    char *buf;
    ssize_t ret;

    ret = read(dolog_queue[0], &pid, sizeof(pid));
    assert(ret == sizeof(pid));
    ret = read(dolog_queue[0], &status, sizeof(status));
    assert(ret == sizeof(status));
    ret = read(dolog_queue[0], &fmt_len, sizeof(fmt_len));
    assert(ret == sizeof(fmt_len));

    buf = malloc(fmt_len+1);
    assert(buf != NULL);
    ret = read(dolog_queue[0], buf, fmt_len);
    assert(ret == (ssize_t) fmt_len);
    buf[fmt_len] = '\0';

    ++spawn_recursion;
    doLog(buf, pid, status);
    --spawn_recursion;
}

__attribute__ ((noreturn))
static void metalog_signal_exit(int exit_status)
{
    exit_hook();
    _exit(exit_status);
}

__attribute__ ((noreturn))
static RETSIGTYPE sigkchld(int sig)
{
    signal_doLog_queue("Process [%u] died with signal [%d]\n", (unsigned int) getpid(), sig);
    metalog_signal_exit(EXIT_FAILURE);
}

static RETSIGTYPE sigchld(int sig)
{
    pid_t pid;
    signed char should_exit = 0;
    int child_status;

    (void) sig;
    while ((pid = waitpid((pid_t) -1, &child_status, WNOHANG)) > (pid_t) 0) {
        if (pid == child) {
            signal_doLog_queue("Klog child [%u] died.", (unsigned) pid, 0);
            should_exit = 1;
        } else {
            if (WIFEXITED(child_status) && WEXITSTATUS(child_status))
                signal_doLog_queue("Child [%u] exited with return code %u.",
                      (unsigned) pid, WEXITSTATUS(child_status));
            else if (!WIFEXITED(child_status))
                signal_doLog_queue("Child [%u] caught signal %u.",
                      (unsigned) pid, WTERMSIG(child_status));
            else if (verbose)
                signal_doLog_queue("Child [%u] exited successfully.", (unsigned) pid, 0);
        }
    }
    if (should_exit != 0) {
        child = (pid_t) 0;
        metalog_signal_exit(EXIT_FAILURE);
    }
}

static RETSIGTYPE sigusr1(int sig)
{
    (void) sig;

    synchronous = (sig_atomic_t) 1;
    signal_doLog_queue("Got SIGUSR1 - enabling synchronous mode.", 0, 0);
    flushAll();
}

static RETSIGTYPE sigusr2(int sig)
{
    (void) sig;

    synchronous = (sig_atomic_t) 0;
    signal_doLog_queue("Got SIGUSR2 - disabling synchronous mode.", 0, 0);
}

static void setsignals(void)
{
    atexit(exit_hook);
    signal(SIGCHLD, sigchld);
    signal(SIGPIPE, sigkchld);
    signal(SIGHUP, sigkchld);
    signal(SIGTERM, sigkchld);
    signal(SIGQUIT, sigkchld);
    signal(SIGINT, sigkchld);
    signal(SIGUSR1, sigusr1);
    signal(SIGUSR2, sigusr2);
}

#ifndef HAVE_DAEMON
static int closedesc_all(const int closestdin)
{
    int fodder;

    if (closestdin != 0) {
        (void) close(0);
        if ((fodder = open("/dev/null", O_RDONLY)) == -1) {
            return -1;
        }
        (void) dup2(fodder, 0);
        if (fodder > 0) {
            (void) close(fodder);
        }
    }
    if ((fodder = open("/dev/null", O_WRONLY)) == -1) {
        return -1;
    }
    (void) dup2(fodder, 1);
    (void) dup2(1, 2);
    if (fodder > 2) {
        (void) close(fodder);
    }

    return 0;
}
#endif

static void dodaemonize(void)
{
    if (daemonize != 0) {
#ifndef HAVE_DAEMON
        pid_t child;
        if ((child = fork()) == (pid_t) -1) {
            warnp("Daemonization failed - fork");
            return;
        } else if (child != (pid_t) 0) {
            _exit(EXIT_SUCCESS);
        } else if (setsid() == (pid_t) -1) {
            errp("Daemonization failed : setsid");
        }
        (void) chdir("/");
        (void) closedesc_all(1);
#else
        if (daemon(0, 0))
            errp("Daemonization failed");
#endif
    }
}

static void setgroup(void)
{
        if (group_name == NULL) return;
        struct group *g;
        errno = 0;
        if ((g = getgrnam(group_name)) == NULL) {
            if(errno == 0)
                err("Failed to set group: group '%s' not found", group_name);
            else
                errp("Failed to set group");
        }
        if (setgid(g->gr_gid) == -1)
            errp("Failed to set group");
}

__attribute__ ((noreturn))
static void help(void)
{
    const struct option *options = long_options;

    puts(PACKAGE " version " VERSION "\n");
    puts("Options:");
    do {
        printf("   -%c, --%s%s\n", options->val, options->name,
               options->has_arg ? " <opt>" : "");
        options++;
    } while (options->name != NULL);
    exit(EXIT_SUCCESS);
}

static void parseOptions(int argc, char *argv[])
{
    int fodder;
    int option_index = 0;

    while ((fodder =  getopt_long(argc, argv, GETOPT_OPTIONS,
                                  long_options, &option_index)) != -1) {
        switch (fodder) {
        case 'a' :
            synchronous = (sig_atomic_t) 0;
            break;
        case 'B' :
            daemonize = 1;
            break;
#ifdef HAVE_KLOGCTL
        case 'c' :
            console_level = atoi(optarg);
            if (console_level < MIN_CONSOLE_LEVEL ||
                console_level > MAX_CONSOLE_LEVEL)
                err("Invalid console level");
            break;
#endif
        case 'C' :
            config_file = xstrdup(optarg);
            break;
        case 'g' :
            group_name = xstrdup(optarg);
            break;
        case 'v' :
            ++verbose;
            break;
        case 'N' :
            do_kernel_log = false;
            break;
        case 'V' :
            puts(PACKAGE " version " VERSION);
            exit(EXIT_SUCCESS);
        case 'h' :
            help();
        case 'p' :
            pid_file = xstrdup(optarg);
            break;
        case 's' :
            break;
        case ':' :
            err("Option '%c' is missing parameter", optopt);
        case '?':
            err("Unknown option '%c' or argument missing", optopt);
        default :
            err("Unknown option '%c'", optopt);
        }
    }
}

static void checkRoot(void)
{
    if (geteuid() != (uid_t) 0)
        err("Sorry, you must be root to launch this program");
}

int main(int argc, char *argv[])
{
    int sockets[2];

    remote_host.port = DEFAULT_UPD_PORT;
    remote_host.sock = -1;
    remote_host.last_dns.tv_sec = 0;
    remote_host.result = NULL;
    parseOptions(argc, argv);
    if (configParser(config_file) < 0)
        err("Bad configuration file");
    checkRoot();
    setgroup();
    dodaemonize();
    setsignals();
    if (update_pid_file(pid_file))
        return -1;
    clearargs(argc, argv);
    setprogname(PROGNAME_MASTER);
    if (getDataSources(sockets) < 0)
        err("Unable to bind sockets");

    return process(sockets);
}
