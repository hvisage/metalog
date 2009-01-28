EXTRA_DIST = \
	metalog.conf \
	metalog.spec.in \
	metalog.lsm.in \
	m4/gnulib-cache.m4

sysconf_DATA = metalog.conf

SUBDIRS = \
	lib \
	src \
	man

AUTOMAKE_OPTIONS = dist-lzma no-dist-gzip

ACLOCAL_AMFLAGS = -I m4

sf_upload:
	rsync -v --progress $(PACKAGE)-$(VERSION).tar.lzma frs.sourceforge.net:uploads/
