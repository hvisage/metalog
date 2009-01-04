sbin_PROGRAMS = \
	metalog

AM_CPPFLAGS = -DCONFDIR=\"$(sysconfdir)\"
INCLUDES = -I$(top_srcdir)/lib

metalog_SOURCES = \
	metalog.c \
	metalog.h \
	metalog_p.h
	syslognames.h
metalog_LDADD = $(top_builddir)/lib/libgnu.a
