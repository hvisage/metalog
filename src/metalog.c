#include <config.h>

#define SYSLOG_NAMES 1
#include "metalog.h"
#include "metalog_p.h"
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif

static int parseLine(char * const line, ConfigBlock **cur_block,
                     const ConfigBlock * const default_block,
                     const char ** const errptr, int * const erroffset,
                     pcre * const re_newblock, pcre * const re_newstmt,
                     pcre * const re_comment, pcre * const re_null) {
    int ovector[16];    
    pcre *new_regex;
    const char *keyword;
    const char *value;    
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
    if (pcre_exec(re_null, NULL, line, line_size,
                  0, 0, ovector, sizeof ovector / sizeof ovector[0]) >= 0 ||
        pcre_exec(re_comment, NULL, line, line_size,
                  0, 0, ovector, sizeof ovector / sizeof ovector[0]) >= 0) {
        return 0;
    } 
    if (pcre_exec(re_newblock, NULL, line, line_size,
                  0, 0, ovector, sizeof ovector / sizeof ovector[0]) >= 0) {
        ConfigBlock *previous_block = *cur_block;
        
        if ((*cur_block = malloc(sizeof **cur_block)) == NULL) {
            perror("Oh no! More memory!");
            return -3;
        }
        **cur_block = *default_block;
        if (config_blocks == NULL) {
            config_blocks = *cur_block;
        } else {
            previous_block->next_block = *cur_block;
        }
        return 0;
    }
    if ((stcount = 
         pcre_exec(re_newstmt, NULL, line, line_size,
                   0, 0, ovector, 
                   sizeof ovector / sizeof ovector[0])) >= 3) {
        pcre_get_substring(line, ovector, stcount, 1, &keyword);
        pcre_get_substring(line, ovector, stcount, 2, &value);
        if (strcasecmp(keyword, "minimum") == 0) {
            (*cur_block)->minimum = atoi(value);
        } else if (strcasecmp(keyword, "maximum") == 0) {
            (*cur_block)->maximum = atoi(value);
        } else if (strcasecmp(keyword, "facility") == 0) {
            int n = 0;
            int *new_facilities;
            
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
                fprintf(stderr, "Unknown facility : [%s]\n", value);
                return -4;
            }
            if ((*cur_block)->facilities == NULL) {
                if (((*cur_block)->facilities =
                     malloc(sizeof *((*cur_block)->facilities))) == NULL) {
                    perror("Oh no! More memory!");
                    return -3;
                }                    
            } else {
                if ((new_facilities =
                     realloc((*cur_block)->facilities,
                             ((*cur_block)->nb_facilities + 1) *
                             sizeof *((*cur_block)->facilities))) == NULL) {
                    perror("Oh no! More memory!");
                    return -3;
                }                    
            }
            (*cur_block)->facilities[(*cur_block)->nb_facilities] = 
                LOG_FAC(facilitynames[n].c_val);
            (*cur_block)->nb_facilities++;
        } else if (strcasecmp(keyword, "regex") == 0 ||
                   strcasecmp(keyword, "neg_regex") == 0) {
            const char *regex;
            RegexWithSign *new_regexeswithsign;
            
            if ((regex = strdup(value)) == NULL) {
                perror("Oh no! More memory!");
                return -3;
            }
            if ((new_regexeswithsign = 
                 realloc((*cur_block)->regexeswithsign,
                         ((*cur_block)->nb_regexes + 1) *
                         sizeof *((*cur_block)->regexeswithsign))) == NULL) {
                perror("Oh no! More memory!");
                return -3;
            }
            (*cur_block)->regexeswithsign = new_regexeswithsign;
            if ((new_regex = pcre_compile(regex, PCRE_CASELESS, 
                                          errptr, erroffset, NULL)) 
                == NULL) {
                fprintf(stderr, "Invalid regex : [%s]\n", regex);
                return -5;
            }
            {
                RegexWithSign * const this_regex = 
                    &((*cur_block)->regexeswithsign[(*cur_block)->nb_regexes]);
                
                if (strcasecmp(keyword, "neg_regex") == 0) {
                    this_regex->sign = REGEX_SIGN_NEGATIVE;
                } else {
                    this_regex->sign = REGEX_SIGN_POSITIVE;
                }
                this_regex->regex.pcre = new_regex;
                this_regex->regex.pcre_extra = 
                    pcre_study(new_regex, 0, errptr);
            }
            (*cur_block)->nb_regexes++;
        } else if (strcasecmp(keyword, "program_regex") == 0 ||
                   strcasecmp(keyword, "program_neg_regex") == 0) {
            const char *regex;
            RegexWithSign *new_regexeswithsign;
            
            if ((regex = strdup(value)) == NULL) {
                perror("Oh no! More memory!");
                return -3;
            }
            if ((new_regexeswithsign = 
                 realloc((*cur_block)->program_regexeswithsign,
                         ((*cur_block)->program_nb_regexes + 1) *
                         sizeof *((*cur_block)->program_regexeswithsign))) 
                == NULL) {
                perror("Oh no! More memory!");
                return -3;
            }
            (*cur_block)->program_regexeswithsign = new_regexeswithsign;
            if ((new_regex = pcre_compile(regex, PCRE_CASELESS, 
                                          errptr, erroffset, NULL)) 
                == NULL) {
                fprintf(stderr, "Invalid program regex : [%s]\n", regex);
                return -5;
            }
            {
                RegexWithSign * const this_regex = 
                    &((*cur_block)->program_regexeswithsign
                      [(*cur_block)->program_nb_regexes]);
                
                if (strcasecmp(keyword, "program_neg_regex") == 0) {
                    this_regex->sign = REGEX_SIGN_NEGATIVE;
                } else {
                    this_regex->sign = REGEX_SIGN_POSITIVE;
                }
                this_regex->regex.pcre = new_regex;
                this_regex->regex.pcre_extra = 
                    pcre_study(new_regex, 0, errptr);
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
        } else if (strcasecmp(keyword, "logdir") == 0) {
            char *logdir;
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
            if ((new_output = malloc(sizeof *new_output)) == NULL ||
                (logdir = strdup(value)) == NULL) {
                if (new_output != NULL) {
                    free(new_output);
                }
                perror("Oh no! More memory!");
                return -3;
            }    
            new_output->directory = logdir;
            new_output->fp = NULL;
            new_output->size = (off_t) 0;
            new_output->maxsize = (*cur_block)->maxsize;
            new_output->maxfiles = (*cur_block)->maxfiles;
            new_output->maxtime = (*cur_block)->maxtime;
            new_output->dt.previous_prg = NULL;
            new_output->dt.previous_info = NULL;            
            new_output->dt.sizeof_previous_prg = (size_t) 0U;
            new_output->dt.sizeof_previous_info = (size_t) 0U;
            new_output->dt.previous_sizeof_prg = (size_t) 0U;
            new_output->dt.previous_sizeof_info = (size_t) 0U;
            new_output->dt.same_counter = 0U;
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
            if (((*cur_block)->command = strdup(value)) == NULL) {
                perror("Oh no! More memory!");
                return -3;
            }
        } else if (strcasecmp(keyword, "program") == 0) {
            if (((*cur_block)->program = strdup(value)) == NULL) {
                perror("Oh no! More memory!");
                return -3;
            }
        }
    }
    return 0;
}

static int configParser(const char * const file)
{
    char line[LINE_MAX];
    FILE *fp;
    const char *errptr;
    pcre *re_newblock;
    pcre *re_newstmt;
    pcre *re_comment;
    pcre *re_null;
    int retcode = 0;
    int erroffset;
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
            NULL,                      /* output */
            NULL,                      /* command */
            NULL,                      /* program */            
            NULL,                      /* program_regexes */
            0,                         /* program_nb_regexes */
            NULL                       /* next_block */
    };
    ConfigBlock *cur_block = &default_block;

    if ((fp = fopen(file, "r")) == NULL) {
        perror("Can't open the configuration file");
        return -2;
    }
    re_newblock = pcre_compile(":\\s*$", 0, &errptr, &erroffset, NULL);
    re_newstmt = pcre_compile("^\\s*(.+?)\\s*=\\s*\"?(.+?)\"?\\s*$", 0, 
                              &errptr, &erroffset, NULL);
    re_comment = pcre_compile("^\\s*#", 0, &errptr, &erroffset, NULL);        
    re_null = pcre_compile("^\\s*$", 0, &errptr, &erroffset, NULL);
    if (re_newblock == NULL || re_newstmt == NULL ||
        re_comment == NULL || re_null == NULL) {
        fprintf(stderr, "Internal error : can't compile parser regexes\n");
        retcode = -1;
        goto rtn;
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        if ((retcode = parseLine(line, &cur_block, &default_block,
                                 &errptr, &erroffset,
                                 re_newblock, re_newstmt,
                                 re_comment, re_null)) != 0) {
            break;
        }
    }
    rtn:
    if (re_newblock != NULL) {
        pcre_free(re_newblock);
    }
    if (re_newstmt != NULL) {
        pcre_free(re_newstmt);
    }
    if (re_comment != NULL) {
        pcre_free(re_comment);
    }
    if (re_null != NULL) {
        pcre_free(re_null);
    }
    fclose(fp);
    
    return retcode;
}

static void clearargs(int argc, char **argv) {
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
            new_environ[env_nb] = strdup(environ[env_nb]);
        }
        environ = new_environ;
    }
# else
    (void) argc;
    (void) argv;
# endif
#endif
}

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

static int getDataSources(int sockets[])
{
    struct sockaddr_un sa;
#ifdef HAVE_KLOGCTL
    int fdpipe[2];
    pid_t pgid;
#endif
    
    if ((sockets[0] = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("Unable to create a local socket");
        return -1;
    }
    sa.sun_family = AF_UNIX;
    if (snprintf(sa.sun_path, sizeof sa.sun_path, "%s", SOCKNAME) < 0) {
        fprintf(stderr, "Socket name too long");
        close(sockets[0]);
        return -2;
    }
    unlink(sa.sun_path);
    if (bind(sockets[0], (struct sockaddr *) &sa, (socklen_t) sizeof sa) < 0) {
        perror("Unable to bind a local socket");
        close(sockets[0]);
        return -1;
    }
    chmod(sa.sun_path, 0666);
    sockets[1] = -1;
#ifdef HAVE_KLOGCTL
    if (pipe(fdpipe) < 0) {
        perror("Unable to create a pipe");
        close(sockets[0]);
        return -3;
    }    
    pgid = getpgrp();
    if ((child = fork()) < (pid_t) 0) {
        perror("Unable to create the klogd child");
        close(sockets[0]);
        return -3;
    } else if (child == (pid_t) 0) {
        char line[LINE_MAX];        
        int s;
        int t;
        int u;
        ssize_t written;
        size_t towrite;
                
        signal(SIGUSR1, SIG_IGN);
        signal(SIGUSR2, SIG_IGN);
        setpgid((pid_t) 0, pgid);
        setprogname(PROGNAME_KERNEL);
        if (klogctl(1, NULL, 0) < 0 ||
            klogctl(8, NULL, console_level) < 0 ||
            klogctl(6, NULL, 0) < 0) {
            perror("Unable to control the kernel logging device");
            return -4;
        }
        for (;;) {
            while ((s = klogctl(2, line, sizeof line)) < 0 && errno == EINTR);
            t = 0;
            u = 0;
            while (t < s) {
                if (line[t] == '\n') {
                    line[t] = 0;
                    towrite = (size_t) (1U + t - u);
                    do {
                        while ((written = write(fdpipe[1], &line[u], towrite))
                               < (ssize_t) 0 && errno == EINTR);
                        if (written < 0) {
                            break;
                        }
                        if (written >= (ssize_t) towrite) {
                            break;
                        }
                        u += written;                        
                        towrite -= written;
                    } while (towrite > (size_t) 0U);
                    u = ++t;
                } else {
                    t++;
                }
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

static int parseLogLine(const LogLineType loglinetype, char *line,
                        int * const logcode, char ** const date,
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
        const char *months[] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        const time_t now = time(NULL);
        struct tm *tm;
        static char datebuf[100];
        
        *logcode |= LOG_KERN;
        *prg = CF_PROGNAME_KERNEL;
        *info = line;
        if ((tm = localtime(&now)) == NULL) {
            *datebuf = 0;
        } else {
            snprintf(datebuf, sizeof datebuf, "%s %2d %02d:%02d:%02d",
                     months[tm->tm_mon], tm->tm_mday,
                     tm->tm_hour, tm->tm_min, tm->tm_sec);
        }
        *date = datebuf;
                 
        return 0;
    }    
    *date = line;
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
    char path[MAXPATHLEN];
    char old_name[MAXPATHLEN];
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
        fprintf(stderr, "Unable to rotate [%s]\n", directory);
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
            fprintf(stderr, "Path to long to unlink [%s/%s]\n", 
                    directory, old_name);
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
    if (sizeof_info == output->dt.previous_sizeof_info &&
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
        
        if ((pp = realloc(output->dt.previous_info, sizeof_info)) != NULL) {
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
        
        if ((pp = realloc(output->dt.previous_prg, sizeof_prg)) != NULL) {
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
        char path[MAXPATHLEN];    
        
        testdir:
        if (stat(output->directory, &st) < 0) {
            if (mkdir(output->directory, OUTPUT_DIR_PERMS) < 0) {
                fprintf(stderr, "Can't create [%s]\n", output->directory);
                return -1;
            }                
        } else if (!S_ISDIR(st.st_mode)) {
            if (unlink(output->directory) < 0) {
                fprintf(stderr, "Can't unlink [%s]\n", output->directory);
                return -1;
            }
            goto testdir;
        }
        if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_CURRENT,
                     output->directory) < 0) {
            fprintf(stderr, "Path name too long for current in [%s]\n", 
                    output->directory);
            return -2;
        }
        if ((fp = fopen(path, "a")) == NULL) {
            fprintf(stderr, "Unable to access [%s]\n", path);
            return -3;
        }
        if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_TIMESTAMP,
                     output->directory) < 0) {
            fprintf(stderr, "Path name too long for timestamp in [%s]\n",
                    output->directory);
            fclose(fp);
            return -2;
        }
        if ((fp_ts = fopen(path, "r")) == NULL) {
            recreate_ts:
            creatime = time(NULL);
            if ((fp_ts = fopen(path, "w")) == NULL) {
                fprintf(stderr, "Unable to write the timestamp [%s]\n", path);
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
            fprintf(stderr, "Unable to close [%s]\n", path);
            return -3;
        }
        output->creatime = creatime;
        output->size = (off_t) ftell(fp);
        output->fp = fp;
    }
    if (output->size >= output->maxsize ||
        now > (output->creatime + output->maxtime)) {
        struct tm *time_gm;    
        char path[MAXPATHLEN];
        char newpath[MAXPATHLEN];            
        
        if (output->fp == NULL) {
            fprintf(stderr, "Internal inconsistency line [%d]\n", __LINE__);
            return -6;
        }
        if ((time_gm = gmtime(&now)) == NULL) {
            fprintf(stderr, "Unable to find the current date\n");
            return -4;
        }
        if (snprintf(newpath, sizeof newpath, 
                     "%s/" OUTPUT_DIR_LOGFILES_PREFIX 
                     "%d-%02d-%02d-%02d:%02d:%02d",
                     output->directory, time_gm->tm_year + 1900,
                     time_gm->tm_mon + 1, time_gm->tm_mday,
                     time_gm->tm_hour + 1, time_gm->tm_min,
                     time_gm->tm_sec) < 0) {
            fprintf(stderr, "Path name too long for new path in [%s]\n", 
                    output->directory);
            return -2;
        }
        if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_CURRENT,
                     output->directory) < 0) {
            fprintf(stderr, "Path name too long for current in [%s]\n", 
                        output->directory);
            return -2;
        }
        rotateLogFiles(output->directory, output->maxfiles);
        fclose(output->fp);
        output->fp = NULL;
        if (rename(path, newpath) < 0 && unlink(path) < 0) {
            fprintf(stderr, "Unable to rename [%s] to [%s]\n",
                    path, newpath);
            return -5;
        }
        if (snprintf(path, sizeof path, "%s/" OUTPUT_DIR_TIMESTAMP,
                     output->directory) < 0) {
            fprintf(stderr, "Path name too long for timestamp in [%s]\n",
                    output->directory);
            return -2;
        }            
        unlink(path);
        goto testdir;
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
    fprintf(output->fp, "%s [%s] %s\n", date, prg, info);
    output->size += (off_t) strlen(date);
    output->size += (off_t) (sizeof_prg + sizeof_info);
    output->size += (off_t) 5;        
    if (synchronous != (sig_atomic_t) 0) {
        fflush(output->fp);
    }
    
    return 0;
}

static int spawnCommand(const char * const command, const char * const date,
                        const char * const prg, const char * const info)
{
    struct stat st;
    
    if (command == NULL || *command == 0 ||
        stat(command, &st) < 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Unable to launch [%s]\n",
                command == NULL ? "null" : command);
        return -1;
    }
    command_child = fork();
    if (command_child == (pid_t) 0) {
        execl(command, command, date, prg, info, (char *) NULL);
        _exit(EXIT_FAILURE);
    } else if (command_child != (pid_t) -1) {
        while (command_child != (pid_t) 0) {
            pause();
        }
    }
    return 0;
}

static int processLogLine(const int logcode, const char * const date,
                          const char * const prg, char * const info)
{
    int ovector[16];
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
                    if (pcre_exec(this_regex->regex.pcre, 
                                  this_regex->regex.pcre_extra,
                                  prg, prg_len, 0, 0, ovector,
                                  sizeof ovector / sizeof ovector[0]) >= 0) {
                        regex_result = 1;
                    }
                } else {
                    if (pcre_exec(this_regex->regex.pcre, 
                                  this_regex->regex.pcre_extra,
                                  prg, prg_len, 0, 0, ovector,
                                  sizeof ovector / sizeof ovector[0]) < 0) {
                        regex_result = 1;
                    }                    
                }
                this_regex++;
                nb_regexes--;                
            } while (nb_regexes > 0);
            if (regex_result == 0) {
                goto nextblock;
            }
        }       
        if ((info_len = (int) strlen(info)) < 0) {
            goto nextblock;
        }
        if ((nb_regexes = block->nb_regexes) > 0 && *info != 0) {
            regex_result = 0;            
            this_regex = block->regexeswithsign;
            do {
                if (this_regex->sign == REGEX_SIGN_POSITIVE) {
                    if (pcre_exec(this_regex->regex.pcre, 
                                  this_regex->regex.pcre_extra,
                                  info, info_len, 0, 0, ovector,
                                  sizeof ovector / sizeof ovector[0]) >= 0) {
                        regex_result = 1;
                    }
                } else {
                    if (pcre_exec(this_regex->regex.pcre, 
                                  this_regex->regex.pcre_extra,
                                  info, info_len, 0, 0, ovector,
                                  sizeof ovector / sizeof ovector[0]) < 0) {
                        regex_result = 1;
                    }                    
                }
                this_regex++;
                nb_regexes--;                
            } while (nb_regexes > 0);
            if (regex_result == 0) {
                goto nextblock;
            }
        }
        if ((size_t) info_len > MAX_LOG_LENGTH) {
            info[MAX_LOG_LENGTH] = 0;
        }
        if (block->output != NULL) {
            writeLogLine(block->output, date, prg, info);
        }
        if (block->command != NULL) {
            spawnCommand(block->command, date, prg, info);
        }        
        nextblock:
        
        block = block->next_block;
    }
    
    return 0;
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

static int process(const int sockets[])
{
    struct pollfd ufds[2];
    struct pollfd *ufdspnt;    
    int nbufds = 0;
    int pollret;
    int event;
    ssize_t readen;
    char line[LINE_MAX];
    int logcode;
    char *date;
    const char *prg;
    char *info;
    LogLineType loglinetype;
    
    if (sockets[0] >= 0) {
        ufds[0].fd = sockets[0];
        ufds[0].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
        ufds[0].revents = 0;
        nbufds++;
    }
    if (sockets[1] >= 0) {
        ufds[nbufds].fd = sockets[1];
        ufds[nbufds].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
        ufds[nbufds].revents = 0;
        nbufds++;        
    }
    for (;;) {
        while ((pollret = poll(ufds, nbufds, -1)) < 0 && errno == EINTR);
        ufdspnt = ufds;
        while (pollret > 0) {            
            if ((event = ufdspnt->revents) == 0) {
                goto nextpoll;
            }
            pollret--;
            ufdspnt->revents = 0;
            if (event != POLLIN) {
                sockerr:
                fprintf(stderr, "Socket error - aborting\n");
                close(ufdspnt->fd);
                return -1;
            }
            while ((readen = read(ufdspnt->fd, line, sizeof line - 1U)) < 0
                   && errno == EINTR);
            if (readen < 0) {
                goto sockerr;
            }
            if (readen == 0) {
                goto nextpoll;
            }
            line[readen] = 0;
            /* Kludge - need to be rewritten */
            if (ufdspnt->fd == sockets[1]) {
                loglinetype = LOGLINETYPE_KLOG;
            } else {
                loglinetype = LOGLINETYPE_SYSLOG;
            }
            sanitize(line);
            if (parseLogLine(loglinetype, line, &logcode,
                             &date, &prg, &info) < 0) {
                goto nextpoll;
            }
            processLogLine(logcode, date, prg, info);
            nextpoll:
            ufdspnt++;
        }
    }
    return 0;
}

static int delete_pid_file(const char * const pid_file)
{
    if (pid_file != NULL) {        
        return unlink(pid_file);
    }
    return 0;
}

static int update_pid_file(const char * const pid_file)
{
    char buf[42];
    int fd;
    size_t towrite;
    ssize_t written;
    
    if (pid_file == NULL || *pid_file != '/') {
        return 0;
    }
    if (SNCHECK(snprintf(buf, sizeof buf, "%lu\n",
                         (unsigned long) getpid()), sizeof buf)) {
        return -1;
    }
    if (unlink(pid_file) != 0 && errno != ENOENT) {
        return -1;
    }
    if ((fd = open(pid_file, O_CREAT | O_WRONLY | O_TRUNC |
                   O_NOFOLLOW, (mode_t) 0644)) == -1) {
        fprintf(stderr, "Unable to create the [%s] pid file : [%s]",
                pid_file, strerror(errno));
        return -1;
    }
    towrite = strlen(buf);
    while ((written = write(fd, buf, towrite)) < (ssize_t) 0 &&
           errno == EINTR);
    if (written < (ssize_t) 0 || (size_t) written != towrite) {
        fprintf(stderr, "Error while writing the [%s] pid file : [%s]",
                pid_file, strerror(errno));        
        (void) ftruncate(fd, (off_t) 0);
        while (close(fd) != 0 && errno == EINTR);
        return -1;
    }
    while (close(fd) != 0 && errno == EINTR);    
    
    return 0;
}

static void exit_hook(void)
{
    if (child > (pid_t) 0) {
        kill(child, SIGTERM);
    }
    (void) delete_pid_file(pid_file);
}

static RETSIGTYPE sigkchld(int sig) __attribute__ ((noreturn));
static RETSIGTYPE sigkchld(int sig)
{
    fprintf(stderr, "Process [%u] died with signal [%d]\n", 
            (unsigned int) getpid(), sig);
    exit(EXIT_FAILURE);
}

static RETSIGTYPE sigchld(int sig)
{    
    pid_t pid;
    signed char should_exit = 0;
    
    (void) sig;
#ifdef HAVE_WAITPID
    while ((pid = waitpid((pid_t) -1, NULL, WNOHANG)) > (pid_t) 0) {
        if (pid == child) {
            fprintf(stderr, "Klog child [%u] died\n", (unsigned) pid);
            should_exit = 1;
        } else if (pid == command_child) {
            command_child = (pid_t) 0;
        }
    }
#else
    while ((pid = wait3(NULL, WNOHANG, NULL)) > (pid_t) 0) {
        if (pid == child) {
            fprintf(stderr, "Klog child [%u] died\n", (unsigned) pid);
            should_exit = 1;
        } else if (pid == command_child) {
            command_child = (pid_t) 0;
        }
    }    
#endif
    if (should_exit != 0) {
        child = (pid_t) 0;
        exit(EXIT_FAILURE);
    }
}

static RETSIGTYPE sigusr1(int sig)
{
    (void) sig;
    
    synchronous = (sig_atomic_t) 1;
}

static RETSIGTYPE sigusr2(int sig)
{
    (void) sig;
    
    synchronous = (sig_atomic_t) 0;
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

static void dodaemonize(void)
{
    pid_t child;
    
    if (daemonize != 0) {
        if ((child = fork()) == (pid_t) -1) {
            perror("Daemonization failed - fork");
            return;
        } else if (child != (pid_t) 0) {
            _exit(EXIT_SUCCESS);
        } else if (setsid() == (pid_t) -1) {
            perror("Daemonization failed : setsid");
        }
        (void) chdir("/");
        (void) closedesc_all(1);
    }    
}

static void help(void) __attribute__ ((noreturn));
static void help(void)
{
    const struct option *options = long_options;
    
    puts("\n" PACKAGE " version " VERSION "\n");
    do {
        printf("-%c\t--%s\t%s\n", options->val, options->name,
               options->has_arg ? "<opt>" : "");
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
                console_level > MAX_CONSOLE_LEVEL) {
                fprintf(stderr, "Invalid console level\n");
                exit(EXIT_FAILURE);
            }
            break;
#endif
        case 'C' :
            if ((config_file = strdup(optarg)) == NULL) {
                perror("You're really running out of memory");
                exit(EXIT_FAILURE);
            }
            break;
        case 'h' :
            help();
        case 'p' :
            if ((pid_file = strdup(optarg)) == NULL) {
                perror("You're really running out of memory");
                exit(EXIT_FAILURE);
            }
            break;
        case 's' :
            break;
        default :
            fprintf(stderr, "Unknown option\n");
            exit(EXIT_FAILURE);
        }
    }
}

static void checkRoot(void)
{
    if (geteuid() != (uid_t) 0) {
        fprintf(stderr, "Sorry, you must be root to launch this program\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    int sockets[2];

    checkRoot();
    parseOptions(argc, argv);
    if (configParser(config_file) < 0) {
        fprintf(stderr, "Bad configuration file - aborting\n");
        return -1;
    }
    dodaemonize();
    setsignals();
    (void) update_pid_file(pid_file);
    clearargs(argc, argv);
    setprogname(PROGNAME_MASTER);
    if (getDataSources(sockets) < 0) {
        fprintf(stderr, "Unable to bind sockets - aborting\n");
        return -1;
    }
    process(sockets);
    
    return 0;
}
