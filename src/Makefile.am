sbin_PROGRAMS = \
	metalog

AM_CPPFLAGS = \
	-DCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/lib

metalog_SOURCES = \
	helpers.h \
	metalog.c \
	metalog.h \
	metalog_p.h
	syslognames.h
metalog_LDADD = $(top_builddir)/lib/libgnu.a
