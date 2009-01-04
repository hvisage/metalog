#!/bin/sh

PATH=/usr/local/src/gnu/gnulib:${PATH}
gnulib-tool --import \
	alloca \
	c-ctype \
	connect \
	dirent \
	errno \
	fcntl \
	getopt \
	inttypes \
	memcmp \
	memcpy \
	memmove \
	memset \
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

autoreconf -i -f
