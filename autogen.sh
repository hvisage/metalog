#!/bin/sh

set -e

PATH=/usr/local/src/gnu/gnulib:${PATH}
rm -rf gnulib/ snippet/
gnulib-tool \
	--source-base=gnulib/lib --m4-base=gnulib/m4 \
	--import \
	alloca \
	c-ctype \
	connect \
	dirent \
	errno \
	fcntl \
	getopt-gnu \
	inttypes \
	pathmax \
	poll \
	printf-posix \
	putenv \
	signal \
	snprintf-posix \
	socket \
	socklen \
	stdarg \
	stdbool \
	stdint \
	string \
	strdup-posix \
	strtoull \
	sys_stat \
	sys_time \
	sys_wait \
	time \
	unistd \
	vfprintf-posix \
	vsnprintf-posix

rm -f gnulib/*/.gitignore
git checkout HEAD^ gnulib/.gitignore || true
git add gnulib || true

autoreconf -i -f

echo "git commit -s -m 'update gnulib/autotool files' INSTALL gnulib"
