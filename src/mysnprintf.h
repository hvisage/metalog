#ifndef __MYSNPRINTF_H__
#define __MYSNPRINTF_H__ 1

int workaround_snprintf(char *str, size_t size, const char *format, ...);

#ifndef HAVE_SNPRINTF
# include "fakesnprintf.h"
#endif
#ifdef CONF_SNPRINTF_TYPE
# if CONF_SNPRINTF_TYPE > 0
#  define SNPRINTF_C99 1
# else
#  define SNPRINTF_OLD 1
# endif
#else
# define SNPRINTF_C99 2
#endif

#ifdef SNPRINTF_IS_NOT_BUGGY
/* SNCHECK() returns 0 if a *snprintf() call was safe */
# ifdef SNPRINTF_C99
#  define SNCHECK(CALL, SIZE) ((CALL) >= ((int) (SIZE)))
# else
#  define SNCHECK(CALL, SIZE) ((CALL) < 0)
# endif
#else
/* Slow wrapper, but it works around totally buggy libc's */
# define SNCHECK(CALL, SIZE) (workaround_ ## CALL)
#endif

#endif
