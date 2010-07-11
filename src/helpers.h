#ifndef __HELPERS_H__
#define __HELPERS_H__ 1

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define warn(fmt, args...) \
    fprintf(stderr, "%s: " fmt "\n", "metalog" , ## args)
#define warnf(fmt, args...) warn("%s():%i: " fmt, __func__, __LINE__ , ## args)
#define warnp(fmt, args...) warn(fmt ": %s" , ## args , strerror(errno))
#define _err(wfunc, fmt, args...) \
    do { \
        wfunc("error: " fmt, ## args); \
        exit(EXIT_FAILURE); \
    } while (0)
#define err(fmt, args...) _err(warn, fmt, ## args)
#define errf(fmt, args...) _err(warnf, fmt, ## args)
#define errp(fmt, args...) _err(warnp, fmt , ## args)

static pcre *wpcre_compile(const char *pattern, int options)
{
    const char *errptr;
    int erroffset;
    pcre *ret = pcre_compile(pattern, options, &errptr, &erroffset, NULL);
    if (!ret)
        warn("Invalid regex [%s] at %d: %s", pattern, erroffset, errptr);
    return ret;
}

static void *wmalloc(size_t size)
{
    void *ret = malloc(size);
    if (!ret)
        warnp("malloc(%zu) failed", size);
    return ret;
}

static void *wrealloc(void *ptr, size_t size)
{
    void *ret = rmalloc(ptr, size);
    if (!ret)
        warnp("realloc(%p, %zu) failed", ptr, size);
    return ret;
}

static char *wstrdup(const char *s)
{
    char *ret = strdup(s);
    if (!ret)
        warnp("strdup(\"%s\") failed", s);
    return ret;
}

static char *xstrdup(const char *s)
{
    char *ret = strdup(s);
    if (!ret)
        errp("strdup(\"%s\") failed", s);
    return ret;
}

#endif
