#include <config.h>

#define SYSLOG_NAMES 1
#include "metalog.h"
#include "metalog_p.h"
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif

static int configParser(const char * const file)
{
    pcre *re_newblock;
    pcre *re_newstmt;
    pcre *re_str;
    pcre *re_comment;    
    pcre *re_null;
    const char *errptr;  
    const char *keyword;
    const char *value;
    int erroffset;
    int ovector[16];
    int stcount;
    int retcode = 0;
    FILE *fp;
    ConfigBlock default_block = {
            DEFAULT_MINIMUM,           /* minimum */
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
            NULL                       /* next_block */
    };
    ConfigBlock *cur_block = &default_block;
    pcre *new_regex;
    int line_size;
    char line[LINE_MAX];

    if ((fp = fopen(file, "rt")) == NULL) {
        perror("Can't open the configuration file");
        return -2;
    }
    re_newblock = pcre_compile(":\\s*$", 0, &errptr, &erroffset, NULL);
    re_newstmt = pcre_compile("^\\s*(.+?)\\s*=\\s*\"?(.+?)\"?\\s*$", 0, 
                              &errptr, &erroffset, NULL);
    re_str = pcre_compile("\"(.+)\"", 0, &errptr, &erroffset, NULL);    
    re_comment = pcre_compile("^\\s*#", 0, &errptr, &erroffset, NULL);        
    re_null = pcre_compile("^\\s*$", 0, &errptr, &erroffset, NULL);
    if (re_newblock == NULL || re_newstmt == NULL ||
        re_comment == NULL || re_null == NULL) {
        fprintf(stderr, "Internal error : can't compile parser regexes\n");
        retcode = -1;
        goto rtn;
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        if ((line_size = (int) strlen(line)) <= 0) {
            continue;
        }
        if (line[line_size - 1] == '\n') {
            if (line_size < 2) {
                continue;
            }
            line[--line_size] = 0;
        }
        if (pcre_exec(re_null, NULL, line, line_size,
                      0, 0, ovector, sizeof ovector / sizeof ovector[0]) >= 0 ||
            pcre_exec(re_comment, NULL, line, line_size,
                      0, 0, ovector, sizeof ovector / sizeof ovector[0]) >= 0) {
            continue;
        } 
        if (pcre_exec(re_newblock, NULL, line, line_size,
                      0, 0, ovector, sizeof ovector / sizeof ovector[0]) >= 0) {
            ConfigBlock *previous_block = cur_block;
            
            if ((cur_block = malloc(sizeof *cur_block)) == NULL) {
                perror("Oh no ! More memory !");
                retcode = -3;
                goto rtn;
            }
            *cur_block = default_block;
            if (config_blocks == NULL) {
                config_blocks = cur_block;
            } else {
                previous_block->next_block = cur_block;
            }
            continue;
        }
        if ((stcount = 
             pcre_exec(re_newstmt, NULL, line, line_size,
                       0, 0, ovector, 
                       sizeof ovector / sizeof ovector[0])) >= 3) {
            pcre_get_substring(line, ovector, stcount, 1, &keyword);
            pcre_get_substring(line, ovector, stcount, 2, &value);
            if (strcasecmp(keyword, "minimum") == 0) {
                cur_block->minimum = atoi(value);
            } else if (strcasecmp(keyword, "facility") == 0) {
                int n = 0;
                int *new_facilities;
                
                if (*value == '*' && value[1] == 0) {
                    if (cur_block->facilities != NULL) {
                        free(cur_block->facilities);
                    }
                    cur_block->facilities = NULL;
                    cur_block->nb_facilities = 0;
                    continue;
                }
                while (facilitynames[n].c_name != NULL &&
                       strcasecmp(facilitynames[n].c_name, value) != 0) {
                    n++;
                }
                if (facilitynames[n].c_name == NULL) {
                    fprintf(stderr, "Unknown facility : [%s]\n", value);
                    retcode = -4;
                    goto rtn;
                }
                if (cur_block->facilities == NULL) {
                    if ((cur_block->facilities =
                         malloc(sizeof *(cur_block->facilities))) == NULL) {
                        perror("Oh no ! More memory !");
                        retcode = -3;
                        goto rtn;
                    }                    
                } else {
                    if ((new_facilities =
                         realloc(cur_block->facilities,
                                 (cur_block->nb_facilities + 1) *
                                 sizeof *(cur_block->facilities))) == NULL) {
                        perror("Oh no ! More memory !");
                        retcode = -3;
                        goto rtn;
                    }                    
                }
                cur_block->facilities[cur_block->nb_facilities] = 
                    LOG_FAC(facilitynames[n].c_val);
                cur_block->nb_facilities++;
            } else if (strcasecmp(keyword, "regex") == 0) {
                const char *regex;
                PCREInfo *new_regexes;
                
                if ((regex = strdup(value)) == NULL) {
                    perror("Oh no ! More memory !");
                    retcode = -3;
                    goto rtn;
                }
                if (cur_block->regexes == NULL) {
                    if ((cur_block->regexes = 
                         malloc(sizeof *(cur_block->regexes))) == NULL) {
                        perror("Oh no ! More memory !");
                        retcode = -3;
                        goto rtn;
                    }                    
                } else {
                    if ((new_regexes = 
                         realloc(cur_block->regexes, 
                                 (cur_block->nb_regexes + 1) *
                                 sizeof *(cur_block->regexes))) == NULL) {
                        perror("Oh no ! More memory !");
                        retcode = -3;
                        goto rtn;
                    }
                    cur_block->regexes = new_regexes;
                }
                if ((new_regex = pcre_compile(regex, PCRE_CASELESS, &errptr, &erroffset, NULL)) == NULL) {
                    fprintf(stderr, "Invalid regex : [%s]\n", regex);
                    return -5;
                }
                {
                    PCREInfo * const pcre_info = &cur_block->regexes[cur_block->nb_regexes];
                    
                    pcre_info->pcre = new_regex;
                    pcre_info->pcre_extra = pcre_study(new_regex, 0, &errptr);
                }
                cur_block->nb_regexes++;
            } else if (strcasecmp(keyword, "maxsize") == 0) {
                cur_block->maxsize = (off_t) strtoull(value, NULL, 0);
                if (cur_block->output != NULL) {
                    cur_block->output->maxsize = cur_block->maxsize;
                }                
            } else if (strcasecmp(keyword, "maxfiles") == 0) {
                cur_block->maxfiles = atoi(value);
                if (cur_block->output != NULL) {
                    cur_block->output->maxfiles = cur_block->maxfiles;
                }                
            } else if (strcasecmp(keyword, "maxtime") == 0) {
                cur_block->maxtime = (time_t) strtoull(value, NULL, 0);
                if (cur_block->output != NULL) {
                    cur_block->output->maxtime = cur_block->maxtime;
                }
            } else if (strcasecmp(keyword, "logdir") == 0) {
                char *logdir;
                Output *outputs_scan = outputs;
                Output *previous_scan = NULL;
                Output *new_output;
                
                while (outputs_scan != NULL) {
                    if (outputs_scan->directory != NULL && 
                        strcmp(outputs_scan->directory, value) == 0) {
                        cur_block->output = outputs_scan;
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
                    perror("Oh no ! More memory !");
                    retcode = -3;
                    goto rtn;
                }    
                new_output->directory = logdir;
                new_output->fp = NULL;
                new_output->size = (off_t) 0;
                new_output->maxsize = cur_block->maxsize;
                new_output->maxfiles = cur_block->maxfiles;
                new_output->maxtime = cur_block->maxtime;
                new_output->next_output = NULL;
                if (previous_scan != NULL) {
                    previous_scan->next_output = new_output;
                } else {
                    outputs = new_output;
                }
                cur_block->output = new_output;
                duplicate_output:
                (void) 0;
            } else if (strcasecmp(keyword, "command") == 0) {
                if ((cur_block->command = strdup(value)) == NULL) {
                    perror("Oh no ! More memory !");
                    retcode = -3;
                    goto rtn;
                }
            } else if (strcasecmp(keyword, "program") == 0) {
                if ((cur_block->program = strdup(value)) == NULL) {
                    perror("Oh no ! More memory !");
                    retcode = -3;
                    goto rtn;
                }
            }
            continue;
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

static void clearargs(char **argv) {
#if defined(__linux__) && !defined(HAVE_SETPROCTITLE)
        if (*argv != NULL) {
                argv++;
        }
        while (*argv != NULL) {         
                memset(*argv, 0, strlen(*argv));
                argv++;
        }
#else
        (void) argv;
#endif
}

static void setprogname(const char * const title)
{
#ifdef HAVE_SETPROCTITLE
        setproctitle("-%s", title);
#elif defined(__linux__)
        if (prg_name != NULL) {
                strncpy(prg_name, title, 31);
                prg_name[31] = 0;               
        }
#else
        (void) title;
#endif
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
        int s;
        int t;
        int u;
        ssize_t written;
        size_t towrite;
        char line[LINE_MAX];
                
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
               (sec  < older_sec))))))))))) {   /* yeah ! */
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
    time_t now = time(NULL);
    
    if (output == NULL || output->directory == NULL) {
        return 0;
    }
    if (output->fp == NULL) {
        struct stat st;
        FILE *fp;
        FILE *fp_ts;
        time_t creatime;
        char path[PATH_MAX];    
        
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
        if ((fp = fopen(path, "at")) == NULL) {
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
        if ((fp_ts = fopen(path, "rt")) == NULL) {
            recreate_ts:
            creatime = time(NULL);
            if ((fp_ts = fopen(path, "wt")) == NULL) {
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
        char path[PATH_MAX];
        char newpath[PATH_MAX];            
        
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
    fprintf(output->fp, "%s [%s] %s\n", date, prg, info);
    output->size += (off_t) strlen(date);
    output->size += (off_t) strlen(prg);        
    output->size += (off_t) strlen(info);
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
        execl(command, command, date, prg, info, NULL);
        _exit(EXIT_FAILURE);
    } else if (command_child != (pid_t) -1) {
        while (command_child != (pid_t) 0) {
            pause();
        }
    }
    return 0;
}

static int processLogLine(const int logcode, const char * const date,
                          const char * const prg, const char * const info)
{
    ConfigBlock *block = config_blocks;
    const int facility = LOG_FAC(logcode);
    const int priority = LOG_PRI(logcode);
    int nb_regexes;
    int info_len;
    int ovector[16];
    PCREInfo *pcre_info;
            
    info_len = strlen(info);
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
        if (priority > block->minimum) {
            goto nextblock;
        }
        if (block->program != NULL && strcasecmp(block->program, prg) != 0) {
            goto nextblock;
        }        
        if ((nb_regexes = block->nb_regexes) > 0 && info_len > 0) {
            pcre_info = block->regexes;
            do {
                if (pcre_exec(pcre_info->pcre, pcre_info->pcre_extra,
                              info, info_len, 0, 0, ovector,
                              sizeof ovector / sizeof ovector[0]) >= 0) {
                    goto regex_ok;
                }
                pcre_info++;
                nb_regexes--;                
            } while (nb_regexes > 0);
            goto nextblock;
        }        
        regex_ok:
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
        if (*line < 32U) {
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
            readen = read(ufdspnt->fd, line, sizeof line - 1U);
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

static void exit_hook(void)
{
    if (child > (pid_t) 0) {
        kill(child, SIGTERM);
    }
}

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

static void dodaemonize(void)
{
        pid_t child;
        
        if (daemonize) {
                if ((child = fork()) < (pid_t) 0) {
                        perror("Daemonization failed - fork");
                        return;
                } else if (child != (pid_t) 0) {
                        _exit(EXIT_SUCCESS);
                } else {
                        if (setsid() == (pid_t) -1) {
                           perror("Daemonization failed : setsid");
                        }
                        chdir("/");
                        if (isatty(1)) {
                                close(1);
                        }
                        if (isatty(2)) {
                                close(2);
                        }
                }
        }       
}

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
        case 'h' :
            help();
        case 's' :
            synchronous = (sig_atomic_t) 1;
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

#ifndef HAVE_SETPROCTITLE
        prg_name = argv[0];
#endif
    checkRoot();
    parseOptions(argc, argv);
    if (configParser(CONFIG_FILE) < 0) {
        fprintf(stderr, "Bad configuration file - aborting\n");
        return -1;
    }
    dodaemonize();
    setsignals();
    clearargs(argv);
    setprogname(PROGNAME_MASTER);
    if (getDataSources(sockets) < 0) {
        fprintf(stderr, "Unable to bind sockets - aborting\n");
        return -1;
    }
    process(sockets);
    
    return 0;
}
